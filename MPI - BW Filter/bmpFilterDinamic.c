#include "bmpBlackWhite.h"
#include "mpi.h"
#include "time.h"

/** Enable output for filtering information */
#define DEBUG_FILTERING 0

/** Show information of input and output bitmap headers */
#define SHOW_BMP_HEADERS 1

/** Id of master process*/
#define MASTER 0

#define END_OF_PROCESSING -1


int main(int argc, char** argv){

	tBitmapFileHeader imgFileHeaderInput;			/** BMP file header for input image */
	tBitmapInfoHeader imgInfoHeaderInput;			/** BMP info header for input image */
	tBitmapFileHeader imgFileHeaderOutput;			/** BMP file header for output image */
	tBitmapInfoHeader imgInfoHeaderOutput;			/** BMP info header for output image */
	char* sourceFileName;							/** Name of input image file */
	char* destinationFileName;						/** Name of output image file */
	int inputFile, outputFile;						/** File descriptors */
	unsigned char *outputBuffer;					/** Output buffer for filtered pixels */
	unsigned char *inputBuffer;						/** Input buffer to allocate original pixels */
	unsigned int rowSize;							/** Number of pixels per row */
	unsigned int threshold;							/** Threshold */
	unsigned int currentRow;						/** Current row being processed */
	unsigned int currentPixel;						/** Current pixel being processed */
	unsigned int readBytes;							/** Number of bytes read from input file */
	unsigned int writeBytes;						/** Number of bytes written to output file */
	unsigned int numPixels;							/** Number of neighbour pixels (including current pixel) */
	tPixelVector vector;							/** Vector of neighbour pixels */
	struct timeval tvBegin, tvEnd;					/** Structs to calculate the total processing time */
	unsigned int rank, numProc;

	/********************************/
	MPI_Status stat;
	int rowsFiltered, rowsSent, grain; //rowsFiltered is the rows filtered by worker
	int rowsReceived; //rowsReceived by master atm
	int* auxVector;
	int requested;
	int totalReceived;
	// Check arguments
	if (argc != 5){
		printf ("Usage: ./bmpFilter sourceFile destinationFile threshold grain\n");
		MPI_Abort(MPI_COMM_WORLD, 0);
	}

	MPI_Init (&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &numProc);

	if(numProc < 3){
		printf("Number of process incorrect, should be greater than 2\n");
		MPI_Abort(MPI_COMM_WORLD, 0);
	}


	if(rank == 0){
		// Get input arguments...
		sourceFileName = argv[1];
		destinationFileName = argv[2];
		threshold = atoi(argv[3]);
		grain = atoi(argv[4]);

		auxVector = (int *) malloc ((numProc-1) * sizeof (int));

		// Init seed
		srand(time(NULL));

		// Show info before processing
		printf ("Applying filter to image %s with threshold %d. Generating image %s\n", sourceFileName, threshold, destinationFileName);

		// Filter process begin
		gettimeofday(&tvBegin, NULL);

		// Read headers from input file
		readHeaders (sourceFileName, &imgFileHeaderInput, &imgInfoHeaderInput);
		readHeaders (sourceFileName, &imgFileHeaderOutput, &imgInfoHeaderOutput);

		// Write header to the output file
		writeHeaders (destinationFileName, &imgFileHeaderOutput, &imgInfoHeaderOutput);

		// Calculate row size for input and output images
		rowSize = (((imgInfoHeaderInput.biBitCount * imgInfoHeaderInput.biWidth) + 31) / 32 ) * 4;

		// Show headers...
		if (!SHOW_BMP_HEADERS){
			printf ("Source BMP headers:\n");
			printBitmapHeaders (&imgFileHeaderInput, &imgInfoHeaderInput);
			printf ("Destination BMP headers:\n");
			printBitmapHeaders (&imgFileHeaderOutput, &imgInfoHeaderOutput);
		}

		// Open source image
		if((inputFile = open(sourceFileName, O_RDONLY)) < 0){
			printf("ERROR: Source file cannot be opened: %s\n", sourceFileName);
			exit(1);
		}

		// Open target image
		if((outputFile = open(destinationFileName, O_WRONLY | O_APPEND, 0777)) < 0){
			printf("ERROR: Target file cannot be open to append data: %s\n", destinationFileName);
			exit(1);
		}

		// Allocate memory to copy the bytes between the header and the image data
		outputBuffer = (unsigned char*) malloc ((imgFileHeaderInput.bfOffBits-BIMAP_HEADERS_SIZE) * sizeof(unsigned char));

		// Copy bytes between headers and pixels
		lseek (inputFile, BIMAP_HEADERS_SIZE, SEEK_SET);
		read (inputFile, outputBuffer, imgFileHeaderInput.bfOffBits-BIMAP_HEADERS_SIZE);
		write (outputFile, outputBuffer, imgFileHeaderInput.bfOffBits-BIMAP_HEADERS_SIZE);

		/*******************************************/

		int totalRows = imgInfoHeaderInput.biHeight;
		MPI_Bcast(&rowSize, 1, MPI_INT, MASTER, MPI_COMM_WORLD); //Communicating workers the size of rows so they can malloc the exact memory
		MPI_Bcast(&threshold, 1, MPI_INT, MASTER, MPI_COMM_WORLD);
		MPI_Bcast(&grain, 1, MPI_INT, MASTER, MPI_COMM_WORLD);
		// Allocate memory for input and output buffers
		inputBuffer = (unsigned char *) malloc (rowSize * totalRows * sizeof (unsigned char));
		outputBuffer = (unsigned char*) malloc (rowSize * totalRows * sizeof(unsigned char));

		unsigned char* aux = inputBuffer;
		unsigned char* auxO = outputBuffer; //Pointers to buffers

		// Read current row data to input buffer
		if ((readBytes = read (inputFile, inputBuffer, totalRows*rowSize)) != totalRows*rowSize){
			showError ("Error while reading from source file");
		}

		//unsigned int rowsToSend = totalRows / (numProc-1); //One process is the master, here we calculate the number of rows for each worker

		int i, j;
		rowsSent = 0;

		for (i = 1; i < numProc; i++){ //for each worker (not included the last) we send the content pointer is currently pointing

			//printf("MASTER : Rows to send: %d to rank: %d \n", rowsToSend, i);		
			MPI_Send(&grain, 1, MPI_INT, i, 1, MPI_COMM_WORLD);
			auxVector[i-1] = rowsSent; //for each worker we store the number of the first row of the ones we sent to it
			printf("MASTER : Just sent from %d to %d to rank %d \n", rowsSent, rowsSent + grain, i);
			rowsSent += grain;
			//printf("MASTER : after first send!!!!!! \n");		
			MPI_Send(aux, grain*rowSize, MPI_UNSIGNED_CHAR, i, 1, MPI_COMM_WORLD);
			aux += rowSize*grain;	
					
		}
		printf("MASTER : Total rows are %d \n", totalRows);
		while(totalReceived < totalRows){
			//recibir las filas procesadas de any source
			MPI_Recv(&rowsFiltered, 1, MPI_INT, MPI_ANY_SOURCE, 1, MPI_COMM_WORLD, &stat);
			//sacar de stat la source
			int source = stat.MPI_SOURCE;
			//colocar el puntero y recibir en la pos adecuada
			printf("MASTER: received work from worker [%d] from row %d to row %d \n", source, auxVector[source-1], auxVector[source-1] + rowsFiltered);
			MPI_Recv(auxO + (auxVector[source-1]*rowSize), rowsFiltered*rowSize, MPI_UNSIGNED_CHAR, source, 1, MPI_COMM_WORLD, &stat);
			totalReceived += rowsFiltered;

			if(rowsSent < totalRows){

				requested = (totalRows - rowsSent) > grain ? grain : (totalRows - rowsSent);
				MPI_Send(&requested, 1, MPI_INT, source, 1, MPI_COMM_WORLD);
				printf("MASTER : Just sent to %d starting at row %d to row %d \n", source, rowsSent, rowsSent+requested);
				printf("MASTER : Rows requested are %d \n", requested);
				MPI_Send(aux, requested*rowSize, MPI_UNSIGNED_CHAR, source, 1, MPI_COMM_WORLD);
				aux += requested*rowSize;
				auxVector[source-1] = rowsSent;
				rowsSent += requested;
			}

			else{
				requested = END_OF_PROCESSING;
				MPI_Send(&requested, 1, MPI_INT, source, 1, MPI_COMM_WORLD);
			}

		}

		// Write to output file
		if ((writeBytes = write (outputFile, outputBuffer, totalRows*rowSize)) != totalRows*rowSize){
			showError ("Error while writing to destination file");
		}

		// Close files
		close (inputFile);
		close (outputFile);

		// End of processing
		gettimeofday(&tvEnd, NULL);

		//printf("Filtering time: %ld.%06ld\n", ((tvEnd.tv_usec + 1000000 * tvEnd.tv_sec) - (tvBegin.tv_usec + 1000000 * tvBegin.tv_sec)) / 1000000,
		//					  	  	  	  	  ((tvEnd.tv_usec + 1000000 * tvEnd.tv_sec) - (tvBegin.tv_usec + 1000000 * tvBegin.tv_sec)) % 1000000);
	}

	if(rank != 0){

		int rowsToProcess;

		MPI_Bcast(&rowSize, 1, MPI_INT, MASTER, MPI_COMM_WORLD); //Communicating workers the size of rows so they can malloc the exact memory
		//printf("WORKER [%d]: after broadcast! \n", rank);	
		//Receiving the threshold
		MPI_Bcast(&threshold, 1, MPI_INT, MASTER, MPI_COMM_WORLD);
		//printf("WORKER [%d]: after broadcast of threshold, whose value is % d! \n", rank, threshold);	
		MPI_Bcast(&grain, 1, MPI_INT, MASTER, MPI_COMM_WORLD);

		do{
			MPI_Recv(&rowsToProcess, 1, MPI_INT, MASTER, 1, MPI_COMM_WORLD, &stat);
			//printf("WORKER[%d]: Rows received: %d \n", rank, rowsToProcess);	
			if(rowsToProcess != END_OF_PROCESSING){
				inputBuffer = (char*)malloc(rowSize*rowsToProcess*sizeof(char));
				unsigned char* aux = inputBuffer;
				MPI_Recv(aux, rowSize*rowsToProcess, MPI_UNSIGNED_CHAR, MASTER, 1, MPI_COMM_WORLD, &stat);

				//printf("WORKER[%d]: gonna filter: %d rows \n", rank, rowsToProcess);

				outputBuffer = (char*)malloc(rowSize*rowsToProcess*sizeof(char));
				unsigned char* auxO = outputBuffer; //Pointers to buffers

				//For each row... 
				for (currentRow=0; currentRow<rowsToProcess; currentRow++){

					// For each pixel in the current row...
					for (currentPixel=0; currentPixel<rowSize; currentPixel++){

						// Current pixel
						numPixels = 0;
						vector[numPixels] = inputBuffer[currentPixel + currentRow * rowSize];
						numPixels++;

						// Not the first pixel
						if (currentPixel > 0){
							vector[numPixels] = inputBuffer[(currentPixel + currentRow * rowSize) - 1];
							numPixels++;
						}

						// Not the last pixel
						if (currentPixel < (imgInfoHeaderInput.biWidth-1)){
							vector[numPixels] = inputBuffer[(currentPixel + currentRow * rowSize) + 1];
							numPixels++;
						}

						// Store current pixel value
						outputBuffer[currentPixel + currentRow * rowSize] = calculatePixelValue(vector, numPixels, threshold, DEBUG_FILTERING);
					}

				}
				
				MPI_Send(&rowsToProcess, 1, MPI_INT, MASTER, 1, MPI_COMM_WORLD);
				MPI_Send(auxO, rowSize*rowsToProcess, MPI_UNSIGNED_CHAR, MASTER, 1, MPI_COMM_WORLD);
				//printf("WORKER [%d]: i've sent my work! \n", rank);	
			}

		}while(rowsToProcess != END_OF_PROCESSING);

		printf("WORKER [%d]: sent to sleep after a hard work\n", rank);	
		
	}

	MPI_Finalize();
}
