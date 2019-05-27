#include "bmpBlackWhite.h"
#include "mpi.h"
#include "time.h"

/** Enable output for filtering information */
#define DEBUG_FILTERING 0

/** Show information of input and output bitmap headers */
#define SHOW_BMP_HEADERS 1

/** Id of master process*/
#define MASTER 0


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
	int rowsFiltered, rowsSent;



		// Check arguments
		if (argc != 4){
			printf ("Usage: ./bmpFilter sourceFile destinationFile threshold\n");
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
			MPI_Bcast(&threshold, 1, MPI_INT, MASTER, MPI_COMM_WORLD);//Communicating workers the threshold
			// Allocate memory for input and output buffers
			inputBuffer = (unsigned char *) malloc (rowSize * totalRows * sizeof (unsigned char));
			outputBuffer = (unsigned char*) malloc (rowSize * totalRows * sizeof(unsigned char));

			unsigned char* aux = inputBuffer;
			unsigned char* auxO = outputBuffer; //Pointers to buffers

			// Read current row data to input buffer
			if ((readBytes = read (inputFile, inputBuffer, totalRows*rowSize)) != totalRows*rowSize){
				showError ("Error while reading from source file");
			}

			unsigned int rowsToSend = totalRows / (numProc-1); //One process is the master, here we calculate the number of rows for each worker

			int i;
			rowsSent = 0; //total amount of rows sent

			for (i = 1; i < numProc-1; i++){ //for each worker (not included the last) we send the content pointer is currently pointing

				//printf("MASTER : Rows to send: %d to rank: %d \n", rowsToSend, i);		
				MPI_Send(&rowsToSend, 1, MPI_INT, i, 1, MPI_COMM_WORLD);
				rowsSent += rowsToSend;
				MPI_Send(aux, rowsToSend*rowSize, MPI_UNSIGNED_CHAR, i, 1, MPI_COMM_WORLD);
				aux += rowSize*rowsToSend;	//pointer 
				//printf("MASTER : Just sent %d pixels to rank %d \n", rowsToSend*rowSize, i);		
			}

			int rowsAux = totalRows - rowsSent; //calculate the number of rows to send to the last worker

			if((rowsAux-rowsToSend) == 0){ //then we send the same rows that we sent to other workers or the rows left
				MPI_Send(&rowsToSend, 1, MPI_INT, i, 1, MPI_COMM_WORLD);
				MPI_Send(aux, rowsToSend*rowSize, MPI_UNSIGNED_CHAR, i, 1, MPI_COMM_WORLD);
				aux +=rowSize*rowsToSend;	
			}

			else{
				MPI_Send(&rowsAux, 1, MPI_INT, i, 1, MPI_COMM_WORLD);
				MPI_Send(aux, rowsAux*rowSize, MPI_UNSIGNED_CHAR, i, 1, MPI_COMM_WORLD);
				aux +=rowSize*rowsAux;
			}


			/*Receiving from workers*/


			for(i = 0; i < numProc-1; i++){
				/*Receiving the rows filtered by any worker*/
				MPI_Recv(&rowsFiltered, 1, MPI_INT, MPI_ANY_SOURCE, 1, MPI_COMM_WORLD, &stat);
				//we need to know which worker is later
				int source = stat.MPI_SOURCE;
				//printf("MASTER: received work from worker [%d]! \n", source);	
				auxO = outputBuffer + (rowsToSend*rowSize*(source-1)); //set pointer at the right place to receive filtered data
				MPI_Recv(auxO, rowSize*rowsFiltered, MPI_UNSIGNED_CHAR, source, 1, MPI_COMM_WORLD, &stat);
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

			printf("Filtering time: %ld.%06ld\n", ((tvEnd.tv_usec + 1000000 * tvEnd.tv_sec) - (tvBegin.tv_usec + 1000000 * tvBegin.tv_sec)) / 1000000,
							  	  	  	  	  ((tvEnd.tv_usec + 1000000 * tvEnd.tv_sec) - (tvBegin.tv_usec + 1000000 * tvBegin.tv_sec)) % 1000000);


		}

		if(rank != 0){

			int rowsToProcess; //number of rows this particular worker will filter

			MPI_Bcast(&rowSize, 1, MPI_INT, MASTER, MPI_COMM_WORLD); //Receiving the rowSize from master
			MPI_Bcast(&threshold, 1, MPI_INT, MASTER, MPI_COMM_WORLD);//Receiving the threshold from master
			MPI_Recv(&rowsToProcess, 1, MPI_INT, MASTER, 1, MPI_COMM_WORLD, &stat);
			//printf("WORKER[%d]: Rows received: %d \n", rank, rowsToProcess);	
			inputBuffer = (char*)malloc(rowSize*rowsToProcess*sizeof(char));
			unsigned char* aux = inputBuffer;
			MPI_Recv(aux, rowSize*rowsToProcess, MPI_UNSIGNED_CHAR, MASTER, 1, MPI_COMM_WORLD, &stat);

			//printf("WORKER[%d]: gonna filter: %d rows \n", rank, rowsToProcess*rowSize);

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
			
			/*Sending back data filtered to master*/

			MPI_Send(&rowsToProcess, 1, MPI_INT, MASTER, 1, MPI_COMM_WORLD);
			MPI_Send(auxO, rowSize*rowsToProcess, MPI_UNSIGNED_CHAR, MASTER, 1, MPI_COMM_WORLD);
			
		}
	
		MPI_Finalize();
}
