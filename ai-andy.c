#include <stdlib.h>

#include "ai-andy.h"
#include "board.h"
#include "config.h"
#include "state.h"
#include "util.h"

PlayerActions_t botActions = {	.playTile = botPlayTile,
					.formChain = botFormChain,
					.mergerSurvivor = botMergerSurvivor,
					.mergerOrder = botMergerOrder,
					.buyStock = botBuyStock,
					.mergerTrade = botMergerTrade,
					.endGame = botEndGame
				};


uint8_t botPlayTile(GameState_t* gs, uint8_t playerNum)
{
	uint8_t numPlayable = 0;
	uint8_t playableTiles[HAND_SIZE] = {TILE_NULL};
	
	//Get list of valid plays
	for(int i = 0; i < HAND_SIZE; i++)
	{
		uint8_t tile = gs->players[playerNum].tiles[i];
		
		//Don't bother checking if tile is null
		if(tile == TILE_NULL)
		{
			continue;
		}
		
		if(isValidTilePlay(gs, tile))
		{
			//increment the count of playable tiles AFTER using
			//it as an index
			playableTiles[numPlayable] = tile;
			numPlayable++;
		}
	}
	
	//Throw an error if we get into a state where we can't play ANY tiles
	//The game should stop this from ever happening
	if(numPlayable == 0)
	{
		PRINT_MSG("No playable tiles");
		HANDLE_UNRECOVERABLE_ERROR(HALE_AI_BADSTATE);
	}
	
//	PRINT_MSG_INT("Have this many tiles to choose from", numPlayable);
	
	uint8_t playThisTile = rand() % numPlayable;
	
	/*PRINT_MSG_INT("Playing tile", playableTiles[playThisTile]);
	PRINT_MSG("Hand is as follows");
	for(i = 0; i < HAND_SIZE; i++)
	{
		printf("%u ", gs->players[playerNum].tiles[i]);
	}
	printf("\n");*/
	
	//Othewise, pick one at random
	uint8_t tile = playableTiles[playThisTile];
//	PRINT_MSG_INT("Playing this tile", tile);
	return tile;
}


chain_t botFormChain(GameState_t* gs, uint8_t playerNum)
{
	uint8_t chainSizes[NUM_CHAINS];
	getChainSizes(gs, chainSizes);
	
	//Get the list of valid chains
	chain_t validChains[NUM_CHAINS] = {};
	int numValid = 0;
	for(int i = 0; i < NUM_CHAINS; i++)
	{
		if(chainSizes[i] == 0)
		{
			validChains[numValid] = i;
			numValid++;
		}
	}
	
	//Throw an error if we get into a state where we can't form a chain
	//The game should stop this from ever happening
	if(numValid == 0)
	{
		HANDLE_UNRECOVERABLE_ERROR(HALE_AI_BADSTATE);
	}
	
	
	//Otherwise, pick one at random
	chain_t formThisChain = validChains[rand() % numValid];
//	PRINT_MSG_INT("Forming this chain", formThisChain);
	return formThisChain;
}


chain_t botMergerSurvivor(GameState_t* gs, uint8_t playerNum, uint8_t* options)
{
	//Easy- just pick one of the provided options at random
	//...but first need to enumerate those options nicely
	chain_t condensedOptions[NUM_CHAINS] = {}; //overkill in size, but hey
	uint8_t numOptions = 0;
	for(int i = 0; i < NUM_CHAINS; i++)
	{
		if(options[i])
		{
			condensedOptions[numOptions] = i;
			numOptions++;
		}
	}
	return condensedOptions[rand() % numOptions];
}


void botMergerOrder(GameState_t* gs, uint8_t playerNum, chain_t survivor, uint8_t* options)
{
	//at most 3 chains can be in this list- the fourth possible is
	//already listed as the survivor
	//Indices of options we can order
	uint8_t optionIdx[3] = {};
	uint8_t numOptions = 0;
	
	for(int i = 0; i < NUM_CHAINS; i++)
	{
		if(options[i] < 0xFF) //FIXME: Magic numbers
		{
			optionIdx[numOptions] = i;
			numOptions++;
		}
	}
	
	//Now do a lazy shuffle- just swap indices once each with a random
	//different index
	for(int i = 0; i < numOptions; i++)
	{
		uint8_t swap = rand() % numOptions;
		uint8_t tmp = optionIdx[i];
		optionIdx[i] = optionIdx[swap];
		optionIdx[swap] = tmp;
	}
	//And write the new indices back
	for(int i = 0; i < numOptions; i++)
	{
		options[optionIdx[i]] = i;
	}
}


void botBuyStock(GameState_t* gs, uint8_t playerNum, uint8_t* toBuy)
{
	//Figure out which stocks you CAN buy
	int numCanBuy[NUM_CHAINS] = {0};
	
	//And how many different chains can be bought
	int numOptions = 0;
	
	chain_t options[NUM_CHAINS] = {};
	
	//This will contain the prices for ALL of the chains- so it can
	//be indexed by a chain_t
	int32_t prices[NUM_CHAINS] = {};
	
	//shortcut instead of having to type out the whole thing
	//Also subtract from this as we "buy" stocks to keep track of whether
	//we can buy others
	int32_t cash = gs->players[playerNum].cash;
	uint8_t chainSizes[NUM_CHAINS] = {};
	
	getChainPricesPerShare(gs, prices, chainSizes);
	
	for(int i = 0; i < NUM_CHAINS; i++)
	{
		//Might as well zero out the toBuy array for good measure
		toBuy[i] = 0;
		
		//If the chain exists and has stock left, see if we could buy any
		if( (prices[i] > 0) && (gs->remainingStocks[i] > 0) && (chainSizes[i] > 1) )
		{
			//How many you could buy could be limited by number
			//left on the market, how many shares you can buy
			//per turn, or how much cash is on hand
			int32_t couldBuy = cash / prices[i];
			numCanBuy[i] = (couldBuy > SHARES_PER_TURN) ? SHARES_PER_TURN : couldBuy;
			if(numCanBuy[i] > gs->remainingStocks[i])
			{
				numCanBuy[i] = gs->remainingStocks[i];
			}
			options[numOptions] = i;
			numOptions++;
		}
	}
	
	//If there are no options of things to buy... don't buy anything
	if(numOptions == 0)
	{
		return;
	}
	
	//Now we know how many of each chain we COULD buy, at most.
	//Next, choose how many we WANT to buy (0-3, technically at most
	//SHARES_PER_TURN)
	int numWantToBuy = rand() % (SHARES_PER_TURN + 1);
	
	//For each share we want to buy, pick a random chain and try to
	//buy it- if we can no longer afford it, look through the rest of
	//the chains and try to buy one of those instead.
	for(int i = 0; i < numWantToBuy; i++)
	{
		uint8_t tryToBuy = rand() % numOptions;
		
		//if we can still afford the option, and there are any
		//left, request to buy it
		if( (prices[options[tryToBuy]] < cash) && (numCanBuy[options[tryToBuy]] > 0) )
		{
			toBuy[options[tryToBuy]]++;
			cash -= prices[tryToBuy];
			numCanBuy[options[tryToBuy]]--;
			
		}
		//Otherwise, see if any of the others work
		else
		{
			for(int j = (tryToBuy + 1) % numOptions; j != tryToBuy; j = (j + 1) % numOptions)
			{
				if( (prices[options[j]] < cash) && (numCanBuy[options[j]] > 0) )
				{
					toBuy[options[j]]++;
					cash -= prices[options[j]];
					numCanBuy[options[j]]--;
					break;
				}
			}
		}
		
		//We could probably add something in here to check if we
		//ran out of money or stocks that we can buy, but there's
		//only 3 iterations of this at most, so it's probably not
		//worth it.
	}
}


void botMergerTrade(GameState_t* gs, uint8_t playerNum, chain_t survivor, chain_t defunct, uint8_t* tradeFor, uint8_t* sell)
{
	//The most stocks we could trade FOR or sell
	//Selling is easy- can sell at most how many you have
	uint8_t maxSell = gs->players[playerNum].stocks[defunct];
	
	//How many you could trade FOR could be defined by how many you have,
	//or it could be defined by how many are available to trade for
	uint8_t maxTradeFor = gs->players[playerNum].stocks[defunct]/2;
	if(gs->remainingStocks[survivor] < maxTradeFor)
	{
		maxTradeFor = gs->remainingStocks[survivor];
	}
	
	//Randomize if we pick how much to trade or how much to buy first
	//I think this prevents a bias...
	int tradeFirst = rand() % 2;
	
	//Perform first action
	//Add one to modulos so we get a range of 0-n, inclusive of n
	if(tradeFirst)
	{
		*tradeFor = rand() % (maxTradeFor + 1);
	}
	else
	{
		*sell = rand() % (maxSell + 1);
	}
	
	//Perform second action
	if(tradeFirst)
	{
		//Again, selling is always limited by how many you have
		*sell = rand() % (maxSell - *tradeFor*2 + 1);
	}
	else
	{
		//Before trading, need to re-check limits
		uint8_t possibleNewLimit = (gs->players[playerNum].stocks[defunct] - *sell)/2;
		if(possibleNewLimit < maxTradeFor)
		{
			maxTradeFor = possibleNewLimit;
		}
		*tradeFor = rand() % (maxTradeFor + 1);
	}
}


uint8_t botEndGame(GameState_t* gs, uint8_t playerNum)
{
	//literally pick a random 0 or 1
	return (uint8_t)(rand() & 0x01);
}
