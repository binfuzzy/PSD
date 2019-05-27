#include "clientGame.h"

void showError(const char *msg){
	perror(msg);
	exit(0);
}

void showReceivedCode (unsigned int code){

	tString string;

		if (DEBUG_CLIENT){

			// Reset
			memset (string, 0, STRING_LENGTH);

			switch(code){

				case TURN_BET:
					strcpy (string, "TURN_BET");
					break;

				case TURN_BET_OK:
					strcpy (string, "TURN_BET_OK");
					break;

				case TURN_PLAY:
					strcpy (string, "TURN_PLAY");
					break;

				case TURN_PLAY_HIT:
					strcpy (string, "TURN_PLAY_HIT");
					break;

				case TURN_PLAY_STAND:
					strcpy (string, "TURN_PLAY_STAND");
					break;

				case TURN_PLAY_OUT:
					strcpy (string, "TURN_PLAY_OUT");
					break;

				case TURN_PLAY_WAIT:
					strcpy (string, "TURN_PLAY_WAIT");
					break;

				case TURN_PLAY_RIVAL_DONE:
					strcpy (string, "TURN_PLAY_RIVAL_DONE");
					break;

				case TURN_GAME_WIN:
					strcpy (string, "TURN_GAME_WIN");
					break;

				case TURN_GAME_LOSE:
					strcpy (string, "TURN_GAME_LOSE");
					break;

				default:
					strcpy (string, "UNKNOWN CODE");
					break;
			}

			printf ("Received:%s\n", string);
		}
}

unsigned int readBet (){

	int i, isValid, bet;
	tString enteredMove;

		// Init...
		bet = 0;

		// While player does not enter a correct bet...
		do{

			// Init...
			bzero (enteredMove, STRING_LENGTH);
			isValid = TRUE;

			// Show input message
			printf ("Enter a bet:");

			// Read move
			fgets(enteredMove, STRING_LENGTH-1, stdin);

			// Remove new-line char
			enteredMove[strlen(enteredMove)-1] = 0;

			// Check if each character is a digit
			for (i=0; i<strlen(enteredMove) && isValid; i++){

				if (!isdigit(enteredMove[i]))
					isValid = FALSE;
			}

			// Entered move is not a number
			if (!isValid){
				printf ("Entered bet is not correct. It must be a number greater than 0\n");
			}

			// Entered move is a number...
			else{

				// Conver entered text to an int
				bet = atoi (enteredMove);
			}

		}while (!isValid);

		printf ("\n");

	return ((unsigned int) bet);
}

unsigned int readOption (){

	int i, isValid, option;
	tString enteredMove;

		// Init...
		option = 0;

		// While player does not enter a correct bet...
		do{

			// Init...
			bzero (enteredMove, STRING_LENGTH);
			isValid = TRUE;

			// Show input message
			printf ("Press %d to hit a card and %d to stand:", PLAYER_HIT_CARD, PLAYER_STAND);

			// Read move
			fgets(enteredMove, STRING_LENGTH-1, stdin);

			// Remove new-line char
			enteredMove[strlen(enteredMove)-1] = 0;

			// Check if each character is a digit
			for (i=0; i<strlen(enteredMove) && isValid; i++){

				if (!isdigit(enteredMove[i]))
					isValid = FALSE;
			}

			// Entered move is not a number
			if (!isValid){
				printf ("Wrong option!\n");
			}

			// Entered move is a number...
			else{

				// Conver entered text to an int
				option = atoi (enteredMove);

				if ((option != PLAYER_HIT_CARD) && (option != PLAYER_STAND)){
					printf ("Wrong option!\n");
					isValid = FALSE;
				}
			}

		}while (!isValid);

		printf ("\n");

	return ((unsigned int) option);
}


int main(int argc, char *argv[]){

	int socketfd;						/** Socket descriptor */
	unsigned int port;					/** Port number (server) */
	struct sockaddr_in server_address;	/** Server address structure */
	char* serverIP;						/** Server IP */
	unsigned int endOfGame = FALSE;				/** Flag to control the end of the game */
	tString playerName;					/** Name of the player */
	unsigned int code;					/** Code */

	unsigned int playerNameLen;		/**Player name length*/

	/*Defined by us*/

	unsigned int stack, bet;
	unsigned int points;
	tDeck hand;

		// Check arguments!
		if (argc != 3){
			fprintf(stderr,"ERROR wrong number of arguments\n");
			fprintf(stderr,"Usage:\n$>%s serverIP port\n", argv[0]);
			exit(0);
		}

		// Get the server address
		serverIP = argv[1];

		// Get the port
		port = atoi(argv[2]);

		// Create socket

		socketfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

		// Check if the socket has been successfully created
		
		if (socketfd < 0)
			showError("ERROR while creating the socket");
	

		// Fill server address structure
		
		memset(&server_address, 0, sizeof(server_address));
		server_address.sin_family = AF_INET;
		server_address.sin_addr.s_addr = inet_addr(serverIP);
		server_address.sin_port = htons(port);

		// Connect with server
		
		if (connect(socketfd, (struct sockaddr *) &server_address, sizeof(server_address)) < 0)
			showError("ERROR while establishing connection");
		
		
		printf ("Connection established with server!\n");

		// Init player's name
		do{
			memset(playerName, 0, STRING_LENGTH);
			printf ("Enter player name:");
			fgets(playerName, STRING_LENGTH-1, stdin);

			// Remove '\n'
			playerName[strlen(playerName)-1] = 0;

		}while (strlen(playerName) <= 2);

		// Init
		

		//Sending length of name...

		playerNameLen = strlen(playerName);
		sendUnsignedInt(socketfd, playerNameLen);

		//...and name
		strcat(playerName, "\0"); //concat a \0 to end the string

		send(socketfd, playerName, playerNameLen+1, 0);


		// Game starts
		printf ("Game starts!\n\n");
		

		// While game continues...
		while (!endOfGame){

			code = receiveUnsignedInt(socketfd);
			stack = receiveUnsignedInt(socketfd);
		
			printf("Your current stack is: %d\n\n", stack);

			while(!code){
				
				bet = readBet();

				sendUnsignedInt(socketfd, bet);

				code = receiveUnsignedInt(socketfd);

				if(!code)
					printf("Bet should be less or equal your stack and MAX_BET \n");

			}

			code = receiveUnsignedInt(socketfd); //TURN PLAY OR WAIT
			points = receiveUnsignedInt(socketfd);
			receivePlayerDeck(socketfd, &hand);

			/*Play hand with bet*/

			while(code != TURN_BET && code != TURN_GAME_WIN && code != TURN_GAME_LOSE){
				
				/*Active player*/

				if(code == TURN_PLAY){
					int opt;

					printf("Your cards are: \n");
					printDeck(&hand);
					printf("So you have %d points\n\n", points);
					
					opt = readOption();

					if(opt == 0)
						sendUnsignedInt(socketfd, TURN_PLAY_STAND);
					else if(opt == 1)
						sendUnsignedInt(socketfd, TURN_PLAY_HIT);

				}

				/*Print your hand in case your points are over 21*/

				else if(code == TURN_PLAY_OUT){
					printf("Your cards are: \n");
					printDeck(&hand);
					printf("So you have %d points\n\n", points);

				}

				/*Passive player*/

				else if (code == TURN_PLAY_WAIT){
					printf("Rival's cards are: \n");
					printDeck(&hand);
					printf("So your rival has %d points. Wait for your turn... \n\n", points);
				}

				code = receiveUnsignedInt(socketfd);
				if(code!=TURN_BET){
					points = receiveUnsignedInt(socketfd);
					receivePlayerDeck(socketfd, &hand);
				}

			}

			/*Check if game is already over*/

			if(code == TURN_GAME_WIN){
				printf("You win!!!\n");
				endOfGame = TRUE;
			}

			if(code == TURN_GAME_LOSE){
				printf("You lose...\n");
				endOfGame = TRUE;
			}
			
		}

	// Close socket
	close (socketfd);
}
