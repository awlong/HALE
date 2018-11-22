#include <stdlib.h>

//For memset
#include <string.h>

#include "ai-greedy.h"
#include "player.h"
#include "board.h"
#include "config.h"
#include "state.h"
#include "util.h"

PlayerActions_t greedyActions = {	.playTile = greedyPlayTile,
					.formChain = greedyFormChain,
					.mergerSurvivor = greedyMergerSurvivor,
					.mergerOrder = greedyMergerOrder,
					.buyStock = greedyBuyStock,
					.mergerTrade = greedyMergerTrade,
					.endGame = greedyEndGame
				};


uint8_t greedyPlayTile(GameState_t* gs, uint8_t playerNum)
{
	//FIXME: Incomplete
	// PRINT_MSG("FIXME: Incomplete");
	return 0;
}


chain_t greedyFormChain(GameState_t* gs, uint8_t playerNum)
{
	//Since we'll NEVER keep worthless stock, just pick the most expensive
	//chain we can form
	//FIXME: Look to see if anybody else has stock in the chain before
	//forming it? (ie, take bonus into account)?
	//FIXME: Incomplete
	// PRINT_MSG("FIXME: Incomplete");
	return CHAIN_NONE;
}


chain_t greedyMergerSurvivor(GameState_t* gs, uint8_t playerNum, uint8_t* options)
{
	//FIXME: Incomplete
	// PRINT_MSG("FIXME: Incomplete");
	return CHAIN_NONE;
}


void greedyMergerOrder(GameState_t* gs, uint8_t playerNum, chain_t survivor, uint8_t* options)
{
	//FIXME: Incomplete
	// PRINT_MSG("FIXME: Incomplete");
}


void greedyBuyStock(GameState_t* gs, uint8_t playerNum, uint8_t* toBuy)
{
	//Iterates through every possible purchase, picks the best for
	//for RIGHT NOW
	
	int32_t bestValue = 0;
	uint8_t buy[3] = {CHAIN_EMPTY, CHAIN_EMPTY, CHAIN_EMPTY};
	
	int32_t sharePrices[NUM_CHAINS];
	uint8_t chainSizes[NUM_CHAINS];
	
	Player_t* p = &(gs->players[playerNum]);
	
	getChainPricesPerShare(gs, sharePrices, chainSizes);
	for(int i = 0; i < NUM_CHAINS; i++)
	{
		//If there are no stocks remaining of that type, or we can't
		//afford a share, OR the chain doesn't exist, don't bother
		//looking further
		if( (chainSizes[i] < 2) || (gs->remainingStocks[i] == 0) || (sharePrices[i] > p->cash) )
		{
			continue;
		}
		//Otherwise, "buy" a share
		p->cash -= sharePrices[i];
		p->stocks[i]++;
		gs->remainingStocks[i]--;
		
		for(int j = 0; j < NUM_CHAINS; j++)
		{
			//Need inner loops to execute, so add a method for
			//skipping simulating a buy
			uint8_t skip1 = ((chainSizes[j] < 2) || (gs->remainingStocks[j] == 0) || (sharePrices[j] > p->cash));
			if(skip1)
			{
				//continue;
			}
			else
			{
				p->cash -= sharePrices[j];
				p->stocks[j]++;
				gs->remainingStocks[j]--;
			}
			
			for(int k = 0; k < NUM_CHAINS; k++)
			{
				uint8_t skip2 = ((chainSizes[k] < 2) || (gs->remainingStocks[k] == 0) || (sharePrices[k] > p->cash));
				if(skip2)
				{
					//continue;
				}
				else
				{
					p->cash -= sharePrices[k];
					p->stocks[k]++;
					gs->remainingStocks[k]--;
				}
				
				//Calculate value of this purchase, record if it's the best
				int32_t val;
				calculatePlayerValue(gs, playerNum, &val);
				//Use >= to encourage actually buying stocks
				if(val >= bestValue)
				{
					bestValue = val;
					buy[0] = i;
					buy[1] = skip1 ? CHAIN_EMPTY : j;
					buy[2] = skip2 ? CHAIN_EMPTY : k;
				}
				
				if(!skip2)
				{
					p->cash += sharePrices[k];
					p->stocks[k]--;
					gs->remainingStocks[k]++;
				}
			}//k
			
			if(!skip1)
			{
				p->cash += sharePrices[j];
				p->stocks[j]--;
				gs->remainingStocks[j]++;
			}
		}//j
		
		//"return" the share
		p->cash += sharePrices[i];
		p->stocks[i]--;
		gs->remainingStocks[i]++;
	}//i
	
	
	//Should now have the best value- format purchase order
	memset(toBuy, 0x00, NUM_CHAINS);
	for(int i = 0; i < 3; i++)
	{
		if(buy[i] < CHAIN_NONE)
		{
			toBuy[buy[i]]++;
		}
	}
}


void greedyMergerTrade(GameState_t* gs, uint8_t playerNum, chain_t survivor, chain_t defunct, uint8_t* tradeFor, uint8_t* sell)
{
	int32_t sharePrices[NUM_CHAINS];
	uint8_t chainSizes[NUM_CHAINS];
	
	uint8_t bestTrade = 0;
	
	//If we only have one share, the loop won't run- greedy option is
	//to sell here, so make that the default
	uint8_t bestSell = gs->players[playerNum].stocks[defunct];
	uint8_t bestValue = 0;
	
	Player_t* p = &(gs->players[playerNum]);
	
	getChainPricesPerShare(gs, sharePrices, chainSizes);
	
	//Need to adjust the board- eliminate the defunct chain, turn it into the suriving chain
	//Otherwise value proposition is off
	//FIXME: This isn't perfect- final size of the surviving chain will be at least one bigger than we think
	for(int i = 0; i < BOARD_TILES; i++)
	{
		if(gs->board[i] == defunct)
		{
			gs->board[i] = survivor;
		}
	}
	
	//Trade on outside loop, so never end up testing an always-wrong
	//"keep one share of defunct chain" option
	for(int i = 0; i < p->stocks[defunct]/2; i++)
	{
		//If there aren't enough shares to trade for, don't bother
		//evaluating this option
		if(i > gs->remainingStocks[survivor])
		{
			break;
		}
		
		//"trade"
		p->stocks[defunct] -= 2*i;
		p->stocks[survivor] += i;
		//No need to deal with remaining stocks- we've done the
		//check we care about on them
		
		
		//We really don't need a loop here- just sell everything
		//we don't trade. Keeping defunct stocks is believed to
		//be dumb by this AI
		int j = p->stocks[defunct]; //p->stocks[defunct] has already been reduced by "trading"
		//"sell"
		p->stocks[defunct] -= j;
		p->cash += j*sharePrices[defunct];
		
		//Calculate value of this option
		int32_t val;
		calculatePlayerValue(gs, playerNum, &val);
		if(val > bestValue)
		{
			bestValue = val;
			bestTrade = i;
			bestSell = j;
		}
		
		//undo "sell"
		p->stocks[defunct] += j;
		p->cash -= j*sharePrices[defunct];
		
		
		//undo the "trade"
		p->stocks[defunct] += 2*i;
		p->stocks[survivor] -= i;
	}
	
	//Place trade request, now that the best has been found
	*tradeFor = bestTrade;
	*sell = bestSell;
}


uint8_t greedyEndGame(GameState_t* gs, uint8_t playerNum)
{
	//If we're winning, end the game. Otherwise, don't.
	int32_t maxPlayerValue = 0;
	int32_t ourValue = 0;
	
	calculatePlayerValue(gs, playerNum, &ourValue);
	
	for(int i = 0; i < gs->numPlayers; i++)
	{
		//Don't count ourselves- save some cycles
		if(i == playerNum)
		{
			continue;
		}
		
		int32_t pv = 0;
		calculatePlayerValue(gs, i, &pv);
		
		if(pv > maxPlayerValue)
		{
			maxPlayerValue = pv;
		}
	}
	
	//Return 0 if we're not winning, 1 if we are
	//No, a tie is NOT good enough.
	return (ourValue > maxPlayerValue);
}
