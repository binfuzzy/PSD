#include "game.h"

/** Debug server? */
#define SERVER_DEBUG 1

/** Sockets of a game used by a thread in the server */
typedef struct threadArgs{
	int socketPlayer1;
	int socketPlayer2;
} tThreadArgs;

/**
 * Function that shows an error message.
 *
 * @param msg Error message.
 */
void showError(const char *msg);


/**
 * Function executed by each thread.
 *
 * @param threadArgs Argument that contains the socket of the players.
 */
void *threadProcessing(void *threadArgs);

/**
 * Function that checks if a bet is ok or not.
 *
 * @param bet Argument that contains the player's bet
 * @param stack Argument that contains stack available for the player
 */

int checkBet(unsigned int bet, unsigned int stack);

/**
 * Function that deals two cards for both players.
 *
 * @param session Argument that contains the game info, including players' decks and gameDeck
 */

void dealCards(tSession *session);

/**
 * Function that asks for bet to the player until the bet is correct
 *
 * @param socket Argument that represents the socket of the player
 * @param stack Argument that contains stack available for the player
 */

unsigned int betPlayer(unsigned int socket, unsigned int stack);

/**
 * Function that sends a code, the current points of the hand and the hand itself
 *
 * @param socket Argument that represents the socket of the player
 * @param points Argument of the points of the hand
 * @param deck Argument pointer to the player's hand
 */

void sendStatusTo(unsigned int socket, unsigned int code, unsigned int points, tDeck *deck);

/**
 * Function that sends a code, the current points of the hand and the hand itself
 *
 * @param socketA, socketB Argument that represents the sockets of the players (A is currentPlayer)
 * @param session Argument that contains the game info including gameDeck
 * @param deck Argument pointer to the playerA's hand
 */

void playHand(unsigned int socketA, unsigned int socketB, tDeck *deck, tSession *session);