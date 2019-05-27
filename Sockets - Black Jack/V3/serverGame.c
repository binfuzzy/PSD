#include "serverGame.h"
#include <pthread.h>



void showError(const char *msg){
	perror(msg);
	exit(0);
}

void *threadProcessing(void *threadArgs){

	tSession session;				/** Session of this game */
	int socketPlayer1;				/** Socket descriptor for player 1 */
	int socketPlayer2;				/** Socket descriptor for player 2 */
	tPlayer currentPlayer = player1;			/** Current player */
	int endOfGame = FALSE;					/** Flag to control the end of the game*/

	int playerNameLen;

	// Get sockets for players
	socketPlayer1 = ((tThreadArgs *) threadArgs)->socketPlayer1;
	socketPlayer2 = ((tThreadArgs *) threadArgs)->socketPlayer2;

	printf("\nConnected both players!\n");

	/*Checking players' names*/

	playerNameLen = receiveUnsignedInt (socketPlayer1);

	recv(socketPlayer1, session.player1Name, playerNameLen+1, 0);

	printf("Player 1 name: %s\n", session.player1Name);

	playerNameLen = receiveUnsignedInt (socketPlayer2);

	recv(socketPlayer2, session.player2Name, playerNameLen+1, 0);

	printf("Player 2 name: %s\n\n", session.player2Name);



	printf("Game starts at server!\n");

	initSession(&session);

	while (!endOfGame){

		unsigned int points = 0;

		/*Case current player (active) is Player1*/

		if(currentPlayer == player1){

			printSession(&session);

			/*Get bets of both players and update their stacks*/

			session.player1Bet = betPlayer(socketPlayer1, session.player1Stack);
			session.player1Stack -= session.player1Bet;

			session.player2Bet = betPlayer(socketPlayer2, session.player2Stack);
			session.player2Stack -= session.player2Bet;

			/*Dealing cards*/

			dealCards(&session);

			printSession(&session);
			
			/*First Player1 plays, then Player2*/

			playHand(socketPlayer1, socketPlayer2, &session.player1Deck, &session);

			sendStatusTo(socketPlayer2, TURN_PLAY_RIVAL_DONE, points, &session.player1Deck);

			playHand(socketPlayer2, socketPlayer1, &session.player2Deck, &session);
			
		}

		/*Case current player (active) is Player2*/

		else{

			printSession(&session);

			/*Get bets of both players and update their stacks*/

			session.player2Bet = betPlayer(socketPlayer2, session.player2Stack);
			session.player2Stack -= session.player2Bet;

			session.player1Bet = betPlayer(socketPlayer1, session.player1Stack);
			session.player1Stack -= session.player1Bet;

			/*Dealing cards*/

			dealCards(&session);

			printSession(&session);

			/*First Player1 plays, then Player2*/

			playHand(socketPlayer2, socketPlayer1, &session.player2Deck, &session);

			sendStatusTo(socketPlayer1, TURN_PLAY_RIVAL_DONE, points, &session.player2Deck);

			playHand(socketPlayer1, socketPlayer2, &session.player1Deck, &session);
			
		}

		/*Update stacks after round*/

		updateStacks(&session);

		printSession(&session);

		clearDeck(&session.player1Deck);
		clearDeck(&session.player2Deck);
		initDeck(&session.gameDeck);

		/*Check if anyone has no stack after the round (the game finish) or change the players' rol*/

		if(session.player1Stack == 0){

			sendStatusTo(socketPlayer1, TURN_GAME_LOSE, points, &session.player1Deck);
			sendStatusTo(socketPlayer2, TURN_GAME_WIN, points, &session.player2Deck);
			endOfGame = TRUE;

		}

		else if(session.player2Stack == 0){
			sendStatusTo(socketPlayer2, TURN_GAME_LOSE, points, &session.player2Deck);
			sendStatusTo(socketPlayer1, TURN_GAME_WIN, points, &session.player1Deck);
			endOfGame = TRUE;
		}

		else{
			currentPlayer = getNextPlayer(currentPlayer);
			sendUnsignedInt(socketPlayer1, TURN_BET);
			sendUnsignedInt(socketPlayer2, TURN_BET);
		}


	}

	// Close sockets
	close (socketPlayer1);
	close (socketPlayer2);

	return (NULL) ;
}

int checkBet(unsigned int bet, unsigned int stack){

	return (bet > stack || bet == 0 || bet > MAX_BET) ? TURN_BET : TURN_BET_OK;
	
}

void dealCards(tSession *session){

	unsigned int card;

	card = getRandomCard(&session->gameDeck);
	session->player1Deck.cards[session->player1Deck.numCards] = card;
	session->player1Deck.numCards++;

	card = getRandomCard(&session->gameDeck);
	session->player1Deck.cards[session->player1Deck.numCards] = card;
	session->player1Deck.numCards++;

	card = getRandomCard(&session->gameDeck);
	session->player2Deck.cards[session->player2Deck.numCards] = card;
	session->player2Deck.numCards++;

	card = getRandomCard(&session->gameDeck);
	session->player2Deck.cards[session->player2Deck.numCards] = card;
	session->player2Deck.numCards++;


}

unsigned int betPlayer(unsigned int socket, unsigned int stack){

	sendUnsignedInt(socket, TURN_BET); /*Send to player attached to the socket TURN_BET code*/

	sendUnsignedInt(socket, stack);

	/*Asks for bet until is a valid bet*/

	unsigned int aux = receiveUnsignedInt(socket);

	unsigned int bet365 = checkBet(aux, stack);

	sendUnsignedInt(socket, bet365);

	while(!bet365){

		aux = receiveUnsignedInt(socket);
		bet365 = checkBet(aux, stack);
		sendUnsignedInt(socket, bet365);

	}

	return aux;

}

void sendStatusTo(unsigned int socket, unsigned int code, unsigned int points, tDeck *deck){

	sendUnsignedInt(socket, code);
	sendUnsignedInt(socket, points);
	sendDeckToClient(socket, deck);

}

void playHand(unsigned int socketA, unsigned int socketB, tDeck *deck, tSession *session){

	/*Player 1 plays*/

	unsigned int points = calculatePoints(deck), card;
	sendStatusTo(socketA, TURN_PLAY, points, deck);

	/*Player 2 waits*/
	sendStatusTo(socketB, TURN_PLAY_WAIT, points, deck);

	unsigned int opt = receiveUnsignedInt(socketA);
	unsigned int end = FALSE;
	
	do{

		if(opt == TURN_PLAY_HIT){
			card = getRandomCard(&session->gameDeck);
			deck->cards[deck->numCards] = card;
			deck->numCards++;
			unsigned int points = calculatePoints(deck);

			if(points<=GOAL_GAME){
				sendUnsignedInt(socketA, TURN_PLAY);
				sendUnsignedInt(socketB, TURN_PLAY_WAIT);

			}
			
			else{
				sendUnsignedInt(socketA, TURN_PLAY_OUT);
				sendUnsignedInt(socketB, TURN_PLAY_WAIT);
				end = TRUE;
			}


			sendUnsignedInt(socketA, points);
			sendDeckToClient(socketA, deck);

			/*Player 2 waits*/

			sendUnsignedInt(socketB, points);
			sendDeckToClient(socketB, deck);
			printSession(session);

			if(!end)
				opt = receiveUnsignedInt(socketA);

		}

	}while((opt != TURN_PLAY_STAND) && !end);


}


int main(int argc, char *argv[]){

	int socketfd;						/** Socket descriptor */
	struct sockaddr_in serverAddress;	/** Server address structure */
	unsigned int port;					/** Listening port */
	struct sockaddr_in player1Address;	/** Client address structure for player 1 */
	struct sockaddr_in player2Address;	/** Client address structure for player 2 */
	int socketPlayer1;					/** Socket descriptor for player 1 */
	int socketPlayer2;					/** Socket descriptor for player 2 */
	unsigned int clientLength;			/** Length of client structure */
	tThreadArgs *threadArgs; 			/** Thread parameters */
	pthread_t threadID;					/** Thread ID */


	// Seed
	srand(time(0));

	// Check arguments
	if (argc != 2) {
		fprintf(stderr,"ERROR wrong number of arguments\n");
		fprintf(stderr,"Usage:\n$>%s port\n", argv[0]);
		exit(1);
	}

	// Create the socket
	socketfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	
	// Init server structure
	memset(&serverAddress, 0, sizeof(serverAddress));

	// Get listening port
	port = atoi(argv[1]);

	// Fill server structure
	
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
	serverAddress.sin_port = htons(port);

	 // Bind
	
	 if (bind(socketfd, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) < 0)
		 showError("ERROR while binding");

	 // Listen
	
	listen(socketfd, 10);
	

	// Infinite loop, server is always listening, waiting for a pair of players

	while (1){

		/*create a socket for player 1*/
		clientLength = sizeof(player1Address);
		socketPlayer1 = accept(socketfd, (struct sockaddr *) &player1Address, &clientLength);

		/*create a socket for player 2*/
		clientLength = sizeof(player2Address);
		socketPlayer2 = accept(socketfd, (struct sockaddr *) &player2Address, &clientLength);

		// Allocate memory
		if ((threadArgs = (struct threadArgs *) malloc(sizeof(struct threadArgs))) == NULL)
			showError("Error while allocating memory");

		threadArgs->socketPlayer1 = socketPlayer1;
		threadArgs->socketPlayer2 = socketPlayer2;

		// Create a new thread
		if (pthread_create(&threadID, NULL, threadProcessing, (void *) threadArgs) != 0)
			showError("pthread_create() failed");

		
	}
}
