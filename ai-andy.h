#ifndef HALE_AI_ANDY_H
#define HALE_AI_ANDY_H

#include <stdint.h>

#include "state.h"

/*
 * This AI is a duplicate of random bot used to just generate a series of random game states/logs.
 */


//See player.h for an explanation of these functions

extern PlayerActions_t botActions;



uint8_t botPlayTile(GameState_t* gs, uint8_t playerNum);

chain_t botFormChain(GameState_t* gs, uint8_t playerNum);

chain_t botMergerSurvivor(GameState_t* gs, uint8_t playerNum, uint8_t* options);

void botMergerOrder(GameState_t* gs, uint8_t playerNum, chain_t survivor, uint8_t* options);

void botBuyStock(GameState_t* gs, uint8_t playerNum, uint8_t* toBuy);

void botMergerTrade(GameState_t* gs, uint8_t playerNum, chain_t survivor, chain_t defunct, uint8_t* tradeFor, uint8_t* sell);

uint8_t botEndGame(GameState_t* gs, uint8_t playerNum);


#endif
