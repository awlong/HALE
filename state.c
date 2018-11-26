#include <string.h>

#include "chain.h"
#include "board.h"
#include "config.h"
#include "state.h"
#include "util.h"
#include "player.h"

#ifdef TURN_DELAY
#include <unistd.h>
#endif

//FIXME: Should probably have a better way of doing this
#include "ai-include.h"

#define VERIFY_HALE_STATUS_FATAL(err,msg) {if(err != HALE_OK) {PRINT_MSG(msg); HANDLE_UNRECOVERABLE_ERROR(err);}}

static void initializeGameState(GameState_t* gs)
{
	CHECK_NULL_PTR_FATAL(gs, "gs");
	
	gs->numPlayers = 0;
	gs->currentPlayer = 0;
	
	memset(&(gs->players), 0, MAX_PLAYERS*sizeof(Player_t));
	
	//use a loop here instead of memset, since we might for some ungodly
	//reason not use a single byte for each remaining stock in the future
	for(int i = 0; i < NUM_CHAINS; i++)
	{
		gs->remainingStocks[i] = NUM_STOCKS;
	}
	
	//Ditto for each space on the board
	for(int i = 0; i < BOARD_TILES; i++)
	{
		gs->board[i] = TILE_NULL;
	}
	
	//Initialize and shuffle tiles
	gs->remainingTiles = BOARD_TILES;
	for(int i = 0; i < BOARD_TILES; i++)
	{
		gs->tilePool[i] = i;
	}
	shuffleTiles(gs->tilePool);
	
	//Initialize players (cash, stocks, tiles)
	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		gs->players[i].cash = STARTING_CASH;
		
		memset(gs->players[i].stocks, 0, sizeof(gs->players[i].stocks[0]) * NUM_CHAINS);
		
		for(int j = 0; j < HAND_SIZE; j++)
		{
			gs->players[i].tiles[j] = TILE_NULL;
		}
	}
	
	//Set the current player to something valid
	gs->currentPlayer = 0;
}

static HALE_status_t configurePlayers(GameState_t* gs, uint8_t numPlayers)
{
	CHECK_NULL_PTR_FATAL(gs, "gs");
	
	HALE_status_t err_code = HALE_OK;
	
	if(numPlayers > MAX_PLAYERS)
	{
		PRINT_MSG_INT("Too many players requested", numPlayers);
		return HALE_OOB;
	}
	
	//Need to record the number of players in the game into the actual state
	gs->numPlayers = numPlayers;
	
	//FIXME: Shim to allow us to continue...
	// PRINT_MSG("FIXME: Need real implementation; setting all random players as a shim");
	
	if(numPlayers < 4)
	{
		PRINT_MSG_INT("Number of players must be at least 4 for this shim to work. Players", numPlayers);
		return HALE_BAD_INPUT;
	}
	
	for(int i = 0; i < numPlayers; i++)
	{
		gs->players[i].actions = botActions;
        switch(i)
        {
            case 0:
                gs->players[i].name = "Bot 0";
                break;
            case 1:
                gs->players[i].name = "Bot 1";
                break;
            case 2:
                gs->players[i].name = "Bot 2";
                break;
            case 3:
                gs->players[i].name = "Bot 3";
                break;
        }
	}
	
	for(int i = 0; i < numPlayers; i++)
	{
		//Check that all player actions are non-NULL
		if( (! gs->players[i].actions.playTile) ||
			(! gs->players[i].actions.formChain) ||
			(! gs->players[i].actions.mergerSurvivor) ||
			(! gs->players[i].actions.mergerOrder) ||
			(! gs->players[i].actions.buyStock) ||
			(! gs->players[i].actions.mergerTrade) ||
			(! gs->players[i].actions.endGame) )
		{
			PRINT_MSG_INT("Input player has one or more NULL functions! Player", i);
			return HALE_BAD_INPUT;
		}
	}
	
	return err_code;
	
	//FIXME: End shim
	
	//FIXME
	HANDLE_UNRECOVERABLE_ERROR(HALE_FUNC_NOT_IMPLEMENTED);
	
	return err_code;
}

//Deals <HAND_SIZE> tiles to all players
static HALE_status_t dealInitialTiles(GameState_t* gs)
{
	//This should only be called from runGame(), so any failure in input
	//should be fatal
	CHECK_NULL_PTR_FATAL(gs, "gs");
	
	HALE_status_t err_code = HALE_OK;
	
	for(int i = 0; i < gs->numPlayers; i++)
	{
		for(int j = 0; j < HAND_SIZE; j++)
		{
			err_code = dealTile(gs, i);
			//We won't accept ANY errors in initial deal... but handle that elsewhere
			VERIFY_HALE_STATUS(err_code, "Not accepting ANY errors during initial tile dealing");
		}
	}
	
	return err_code;
}


//Draws <numPlayers> tiles and places them on the board
static HALE_status_t placeInitialTiles(GameState_t* gs)
{
	//This should only be called from runGame(), so any failure in input
	//should be fatal
	CHECK_NULL_PTR_FATAL(gs, "gs");
	
	HALE_status_t err_code = HALE_OK;
	for(int i = 0; i < gs->numPlayers; i++)
	{
		uint8_t tile;
		err_code = drawTile(gs, &tile);
		VERIFY_HALE_STATUS(err_code, "Error drawing tiles for initial board");
		
		//Normally you'd want to call a function to play a tile so it can
		//handle all of the chain creation/merging/etc. Here we literally
		//just want to put them on the board, and explicitly NOT form chains.
		//So we modify the board state directly
		gs->board[tile] = CHAIN_NONE;
	}
	
	return err_code;
}


HALE_status_t makeSanitizedGameStateCopy(GameState_t* gsSanitized, GameState_t* gs, uint8_t playerNum)
{
	CHECK_NULL_PTR(gs, "gs");
	CHECK_NULL_PTR(gsSanitized, "gsSanitized");
	
	//Check if playerNum is in bounds
#ifndef GO_FAST_AND_BREAK_THINGS
	if(playerNum >= gs->numPlayers)
	{
		PRINT_MSG_INT("playerNum is invalid", playerNum);
		return HALE_OOB;
	}
#endif //GO_FAST_AND_BREAK_THINGS
	
	//Board state, stock states, etc. get copied over entirely. They're
	//public information
	gsSanitized->numPlayers = gs->numPlayers;
	memcpy(gsSanitized->board, gs->board, sizeof(gs->board[0])*BOARD_TILES);
	memcpy(gsSanitized->remainingStocks, gs->remainingStocks, sizeof(gs->remainingStocks[0])*NUM_CHAINS);
	gsSanitized->remainingTiles = gs->remainingTiles;
	gsSanitized->currentPlayer = gs->currentPlayer;
	
	//Zero out the remaining tile pool- the players don't get to see that
	memset(gsSanitized->tilePool, 0, sizeof(gsSanitized->tilePool[0])*BOARD_TILES);
	
	//Handle player states specially- each player gets to know their own
	//state, but not the COMPLETE state of the rest of the players
	//Copy over complete states
	memcpy(gsSanitized->players, gs->players, sizeof(gs->players[0])*gs->numPlayers);
	
	//zero out the information you don't get
	for(int i = 0; i < gs->numPlayers; i++)
	{
		//Don't zero out the player's own data
		if(i != playerNum)
		{
			//Other players don't get to see what tiles the other players
			//have, nor what actions the other players have registered
			memset(gsSanitized->players[i].tiles, 0, sizeof(gsSanitized->players[i].tiles[0])*HAND_SIZE);
			memset(&(gsSanitized->players[i].actions), 0, sizeof(gsSanitized->players[i].actions));
		}
	}
	
	return HALE_OK;
}


//Handles getting a tile from the player, and validating that it's valid
//for play
//Returns the tile to be played
static HALE_status_t getTileToPlay(GameState_t* gs, uint8_t* tile)
{
	//This should only be called from runGame(), so this should NEVER
	//happen. Thus being a fatal error.
	CHECK_NULL_PTR_FATAL(gs, "gs");
	
	//Check if gs->currentPlayer is in bounds
#ifndef GO_FAST_AND_BREAK_THINGS
	if(gs->currentPlayer >= gs->numPlayers)
	{
		PRINT_MSG_INT("gs->currentPlayer is invalid", gs->currentPlayer);
		HANDLE_UNRECOVERABLE_ERROR(HALE_OOB);
	}
#endif //GO_FAST_AND_BREAK_THINGS

	//Shorthand
	uint8_t currentPlayer = gs->currentPlayer;

	//Before doing anything involving talking to players, make a sanitized
	//copy of the game state
	GameState_t gsSanitized;
	HALE_status_t err = makeSanitizedGameStateCopy(&gsSanitized, gs, currentPlayer);
	VERIFY_HALE_STATUS(err, "FATAL: Couldn't get sanitized game state for getting tile");

	//First, request a tile to be played from the current player
	*tile = gsSanitized.players[currentPlayer].actions.playTile(&gsSanitized, currentPlayer);
	
	//Validate that the tile is legal:
	//-player had the tile in their hand
	//-tile is a legal move on the board
	uint8_t tileInHand = 0;
	//Check if it's in their hand
	for(int i = 0; i < HAND_SIZE; i++)
	{
		if(gs->players[currentPlayer].tiles[i] == *tile)
		{
			tileInHand = 1;
		}
	}
	
	//If it's not in their hand, OR it's an invalid tile to play... too bad
	if(!isValidTilePlay(gs, *tile) || !tileInHand)
	{
		// PRINT_MSG_INT("Player tried to play invalid tile", *tile);
		// PRINT_MSG_INT("Player", currentPlayer);
		// PRINT_MSG_ARG("Player name", gs->players[currentPlayer].name);
		
		
		//If the tile wasn't valid, too bad- all well-behavied players/AI
		//code should ensure that it is BEFORE sending it to the game. Pick
		//a tile from their hand for them and play that.
		for(int i = 0; i < HAND_SIZE; i++)
		{
			*tile = gs->players[currentPlayer].tiles[i];
			if(isValidTilePlay(gs, *tile))
			{
				break;
			}
		}
		//PRINT_MSG_INT("Playing a tile for them", *tile);
		if(!isValidTilePlay(gs, *tile))
		{
			return HALE_SHOULD_BE_IMPOSSIBLE;
		}
	}
	
	//Find the tile in the player's hand and remove it
	for(int i = 0; i < HAND_SIZE; i++)
	{
		if(gs->players[currentPlayer].tiles[i] == *tile)
		{
			gs->players[currentPlayer].tiles[i] = TILE_NULL;
		}
	} 
	
	//Do NOT deal a new tile- that's a different step
	
	return HALE_OK;
}


//Actually handles the process of doing a merger
static HALE_status_t handleMerger(GameState_t* gs, chain_t survivingChain, chain_t defunctChain)
{
	//Hand out bonuses
	for(int i = 0; i < gs->numPlayers; i++)
	{
		int32_t bonus = 0;
		calculatePlayerBonus(gs, i, defunctChain, &bonus);
		LOG_MSG_INT("Player Bonus", bonus);
		gs->players[i].cash += bonus;
	}
	
	int32_t chainPrices[NUM_CHAINS];
	getChainPricesPerShare(gs, chainPrices, NULL); //FIXME: Error checking
	
	//Starting with the current player (presumed to be the merge-maker), allow
	//exchanging/selling stock
	for(int i = 0; i < gs->numPlayers; i++)
	{
		//current player we're handling
		uint8_t cp = (gs->currentPlayer + i) % gs->numPlayers;
		
		//Ask current player what they want to do
		GameState_t gsSanitized;
		HALE_status_t err = makeSanitizedGameStateCopy(&gsSanitized, gs, cp);
		VERIFY_HALE_STATUS(err, "Couldn't get sanitized game copy for handling merger");
		
		uint8_t tradeFor = 0;
		uint8_t sell = 0;
		gsSanitized.players[cp].actions.mergerTrade(&gsSanitized, cp, survivingChain, defunctChain, &tradeFor, &sell);
		
		//Make sure that the request is valid
		//Player must hold enough stocks to meet the trade/sell request,
		//AND there must be enough remaining stocks of the surviving chain
		//to fulfill the trade
		//Cast to 16 bits to prevent check overflowing...
		if( ((2*((uint16_t)tradeFor) + ((uint16_t)sell)) > (uint16_t)gs->players[cp].stocks[defunctChain]) || (tradeFor > gs->remainingStocks[survivingChain]) )
		{
			//Invalid... not executing your trade.
			PRINT_MSG_INT("Invalid trade request; ignoring. Player", cp);
			continue;
		}
		
		//If the trade is valid (which it must be at this point), execute
		//Sale first
		LOG_MSG_INT("Player Merge Actions", cp);
		LOG_MSG_INT("Sell", sell);
		LOG_MSG_INT("Trade-In", tradeFor);
		gs->players[cp].stocks[defunctChain] -= sell;
		gs->remainingStocks[defunctChain] += sell;
		gs->players[cp].cash += sell * chainPrices[defunctChain];
		
		//Then trade
		gs->players[cp].stocks[defunctChain] -= 2*tradeFor;
		gs->remainingStocks[defunctChain] += 2*tradeFor;
		gs->players[cp].stocks[survivingChain] += tradeFor;
		gs->remainingStocks[survivingChain] -= tradeFor;
		
#ifdef ENABLE_PARANOID_CHECKS
		{
			int8_t defunctStocks = gs->remainingStocks[defunctChain];
			int8_t survivingStocks = gs->remainingStocks[survivingChain];
			if( (defunctStocks > NUM_STOCKS) || (defunctStocks < 0) )
			{
				PRINT_MSG_INT("Ended up with too many/few defunct stocks", defunctStocks);
				PRINT_MSG_ARG("Chain", chainNames[defunctChain]);
				HANDLE_UNRECOVERABLE_ERROR(HALE_SHOULD_BE_IMPOSSIBLE);
			}
			
			if( (survivingStocks > NUM_STOCKS) || (survivingStocks < 0) )
			{
				PRINT_MSG_INT("Ended up with too many/few surviving stocks", survivingStocks);
				PRINT_MSG_ARG("Chain", chainNames[survivingChain]);
				HANDLE_UNRECOVERABLE_ERROR(HALE_SHOULD_BE_IMPOSSIBLE);
			}
		}
#endif
	}// for
	
	return HALE_OK;
}


//Handles when playing a tile would cause a merger
//FIXME: Add some checks to ensure assumptions (eg, checks made in handleTilePlayPhase) are valid here
static HALE_status_t handleTilePlayMerger(GameState_t* gs, uint8_t tile, uint8_t numMergingChains, chain_t* mergingChains)
{
	CHECK_NULL_PTR(gs, "gs");
	CHECK_NULL_PTR(mergingChains, "mergingChains");
	
	HALE_status_t err_code = HALE_OK;
	
	//First, get the CURRENT size of all chains- that's what's
	//used to determine bonuses and whatnot
	uint8_t chainSizes[NUM_CHAINS];
	err_code = getChainSizes(gs, chainSizes);
	VERIFY_HALE_STATUS(err_code, "Couldn't calculate chain sizes for handling tile play merger");
	
	//No matter what you do, you'll need the largest chain/final
	//surviving chain- might as well do that first.
	uint8_t largest[NUM_CHAINS] = {}; //entries set to 1 if largest, 0 otherwise
	uint8_t numLargest = 0;
	uint8_t sizeLargest = 0;
	
	//Print all chains that are in the merger
	for(int i = 0; i < NUM_CHAINS; i++)
	{
		if(mergingChains[i])
		{
			LOG_MSG_ARG("Merging Chains", chainNames[i]);
		}
	}

	//First, find the largest chain that's merging
	for(int i = 0; i < NUM_CHAINS; i++)
	{
		if( (mergingChains[i]) && (chainSizes[i] > sizeLargest) )
		{
			sizeLargest = chainSizes[i];
			// PRINT_MSG_INT("Largest so far", sizeLargest);
		}
	}
#ifdef ENABLE_PARANOID_CHECKS
	//Check if our largest size is valid
	if(sizeLargest < 2)
	{
		PRINT_MSG("Merger, but invalid largest chain size\r\n");
		HANDLE_UNRECOVERABLE_ERROR(HALE_SHOULD_BE_IMPOSSIBLE);
	}
	for(int i = 0; i < NUM_CHAINS; i++)
	{
		if( (mergingChains[i]) && (chainSizes[i] > sizeLargest) )
		{
			PRINT_MSG("Found a chain in the merger larger than the largest!");
			HANDLE_UNRECOVERABLE_ERROR(HALE_SHOULD_BE_IMPOSSIBLE);
		}
	}
#endif
	
	//Then check every chain that's merging against this size
	for(int i = 0; i < NUM_CHAINS; i++)
	{
		if( (mergingChains[i]) && (chainSizes[i] == sizeLargest) )
		{
			numLargest++;
			largest[i] = 1;
		}
	}
	// PRINT_MSG_INT("Got this many chains of largest size", numLargest);
#ifdef ENABLE_PARANOID_CHECKS
	//Check if our largest size is valid
	if(numLargest < 1)
	{
		PRINT_MSG("No chain matches the size of the largest chain involved in the merger!\r\n");
		HANDLE_UNRECOVERABLE_ERROR(HALE_SHOULD_BE_IMPOSSIBLE);
	}
#endif
	
	//Now, IF there's a tie for the largest, need to pick which
	//chain survives
	chain_t survivingChain = CHAIN_NONE;
	if(numLargest > 1)
	{
		//Need to make duplicates of the game state and the options array, to they can't be messed with
		GameState_t gsSanitized;
		err_code = makeSanitizedGameStateCopy(&gsSanitized, gs, gs->currentPlayer);
		VERIFY_HALE_STATUS(err_code, "Couldn't create sanitized state for asking merger survivor");
		
		uint8_t dupLargest[NUM_CHAINS];
		memcpy((void*)dupLargest, (void*)largest, NUM_CHAINS);
		
		survivingChain = gsSanitized.players[gs->currentPlayer].actions.mergerSurvivor(&gsSanitized, gs->currentPlayer, dupLargest);
		
		//Verify the player didn't ask for something invalid
		if( (survivingChain >= CHAIN_NONE) || (!largest[survivingChain]) )
		{
			survivingChain = CHAIN_NONE;
			PRINT_MSG("Player requested invalid surviving chain- picking for them!\r\n");
			//We do this by falling through, which will then pick the first ("only", nominally) "largest" chain and roll with that
			//FIXME: This biases the game towards choosing to always keep the cheaper chains... but the AI could do that anyway if its what it wanted
		}
	}
	//If we don't have a valid survivor request here, either
	//there's only ONE largest chain (and no decision to make),
	//or the player screwed up, and we'll choose for them by
	//picking the first "largest" we find.
	if(survivingChain == CHAIN_NONE)
	{
		// PRINT_MSG("Only one largest (or player screwed up)");
		for(int i = 0; i < NUM_CHAINS; i++)
		{
			if(largest[i])
			{
				survivingChain = i;
				break;
			}
		}
	}
	
	LOG_MSG_ARG("Surviving chain", chainNames[survivingChain]);
#ifdef ENABLE_PARANOID_CHECKS
	if(!largest[survivingChain])
	{
		PRINT_MSG("Just ensured we had a valid survivor for the merger, but no longer do!\r\n");
		HANDLE_UNRECOVERABLE_ERROR(HALE_SHOULD_BE_IMPOSSIBLE);
	}
	if(!mergingChains[survivingChain])
	{
		PRINT_MSG("Supposed survivor wasn't merging in the first place!\r\n");
		HANDLE_UNRECOVERABLE_ERROR(HALE_SHOULD_BE_IMPOSSIBLE);
	}
#endif
	
	//We now know which chain ultimately survives, but may need
	//to handle multiple mergers in a player-selected order-
	//get that order, if necessary
	uint8_t order[NUM_CHAINS];
	
	//Set default order
	memset(order, 0xff, NUM_CHAINS);
	uint8_t currentOrder = 0;
	for(int i = 0; i < NUM_CHAINS; i++)
	{
		//If this chain is supposed to merge, AND it's not the survivor, mark it
		if( (mergingChains[i]) && (i != survivingChain) )
		{
			order[i] = currentOrder;
			currentOrder++;
		}
	}
	
	//Ask player to select merging order
	//FIXME: Need to check chain sizes, and only ask when they can't be
	//ordered largest to smallest? CHECK RULES!
	if(numMergingChains > 2)
	{
		GameState_t gsSanitized;
		err_code = makeSanitizedGameStateCopy(&gsSanitized, gs, gs->currentPlayer);
		VERIFY_HALE_STATUS(err_code, "Couldn't create sanitized state for asking merger order");
		
		uint8_t sanitizedOrder[NUM_CHAINS];
		memcpy((void*)sanitizedOrder, (void*)order, NUM_CHAINS);
		
		gsSanitized.players[gsSanitized.currentPlayer].actions.mergerOrder(&gsSanitized, gsSanitized.currentPlayer, survivingChain, sanitizedOrder);
		
		//Verify order is valid
		//If not, simply don't copy over the real order
		if(!mergerOrderIsValid(gs, tile, survivingChain, sanitizedOrder))
		{
			PRINT_MSG("Player gave bad merger order; picking one for them");
		}
		else
		{
			memcpy((void*)order, (void*)sanitizedOrder, NUM_CHAINS);
		}
	}
	
	//FIXME: Add paranoid check to verify order is OK
	
	//Actually handle the mergers
	for(int i = 0; i < numMergingChains - 1; i++)
	{
		//We already know which chain we're merging with- need to pick out the one we're merging
		//That's the lowest-numbered chain in <order>
		chain_t defunct = 0;
		for(int j = 1; j < NUM_CHAINS; j++)
		{
			if(order[j] < order[defunct])
			{
				defunct = j;
			}
		}
		//We've got the chain to merge, take it off the list
		order[defunct] = 0xff;
		//Handle merger- award cash, exchange stocks
		handleMerger(gs, survivingChain, defunct); //FIXME: error checking
	}
	
	//Now need to update the board
	//Place the one tile on the board as the final chain type
	gs->board[tile] = survivingChain;
	//Iterate through every tile; if one of the defunct chains, update to new chain
	for(int i = 0; i < BOARD_TILES; i++)
	{
		for(int j = 0; j < NUM_CHAINS; j++)
		{
			if( (gs->board[i] < CHAIN_NONE) && (mergingChains[gs->board[i]]) )
			{
				gs->board[i] = survivingChain;
				break;
			}
		}
	}
	
	//Remember there may be non-chain tiles attached! Do a flood-fill on placed tile to clean any of those up
	floodFillNonChain(gs, tile);
	
	return err_code;
}


//Handles when playing a tile would create a chain
//FIXME: Add some checks to ensure assumptions (eg, checks made in handleTilePlayPhase) are valid here
static HALE_status_t handleTilePlayCreate(GameState_t* gs, uint8_t tile)
{
	CHECK_NULL_PTR(gs, "gs");
	
	HALE_status_t err_code = HALE_OK;
	
	GameState_t gsSanitized;
	err_code = makeSanitizedGameStateCopy(&gsSanitized, gs, gs->currentPlayer);
	VERIFY_HALE_STATUS(err_code, "Couldn't create sanitized state for creating chain");
	
	//Request chain to form from the player
	chain_t chainToCreate = gsSanitized.players[gsSanitized.currentPlayer].actions.formChain(&gsSanitized, gsSanitized.currentPlayer);
	
	//Before doing anything, validate that the requested chain can, in fact, be created
	uint8_t chainSizes[NUM_CHAINS];
	getChainSizes(gs, chainSizes);
	if( (chainToCreate >= CHAIN_NONE) || (chainSizes[chainToCreate] > 0))
	{
		//PRINT_MSG_INT("Requested to form invalid chain; picking one for them... requested", chainToCreate);
		//NOTE: i defined external to loop because of paranoid check
		int i;
		for(i = 0; i < NUM_CHAINS; i++)
		{
			if(chainSizes[i] == 0)
			{
				chainToCreate = i;
				break;
			}
		}
#ifdef ENABLE_PARANOID_CHECKS
		if(i >= NUM_CHAINS)
		{
			PRINT_MSG("Should-be-impossible: Can't find chain to form, but tile identified as valid to play");
			HANDLE_UNRECOVERABLE_ERROR(HALE_SHOULD_BE_IMPOSSIBLE);
		}
#endif
	}
	
	//Place tile on the board
	LOG_MSG_ARG("Creating Chain", chainNames[chainToCreate]);
	gs->board[tile] = chainToCreate;
	floodFillNonChain(gs, tile);
	
	//Give the founder a share of stock, if possible
	if(gs->remainingStocks[chainToCreate])
	{
		LOG_MSG_INT(chainNames[chainToCreate], 1);
		gs->players[gs->currentPlayer].stocks[chainToCreate]++;
		gs->remainingStocks[chainToCreate]--;
	}
	
	return err_code;
}


//Handles a basic tile play- either doing nothing or exanding an existing chain
static HALE_status_t handleTilePlayRegular(GameState_t* gs, uint8_t tile)
{
	CHECK_NULL_PTR(gs, "gs");
	
	HALE_status_t err_code = HALE_OK;
	
	chain_t adj[4];
	err_code = getAdjacentSquares(gs, tile, adj);
	VERIFY_HALE_STATUS(err_code, "Couldn't get adjacent squares");
	chain_t newType = CHAIN_NONE;
	
	//Figure out what chain this tile should be a part of
	for(int i = 0; i < 4; i++)
	{
		if(adj[i] < CHAIN_NONE)
		{
#ifdef ENABLE_PARANOID_CHECKS
			if( (newType != CHAIN_NONE) && (newType != adj[i]) )
			{
				PRINT_MSG("Looks like an unexpected merger!");
				HANDLE_UNRECOVERABLE_ERROR(HALE_BAD_INPUT);
			}
#endif
			newType = adj[i];
		}
	}
	
	//FIXME: Add a secondary paranoid check to make sure we
	//didn't screw anything up here
	
	//Assign the square to the proper chain
	gs->board[tile] = newType;
	
	//Update connected tiles with the new chain, if there is one
	if(newType != CHAIN_NONE)
	{
		err_code = floodFillNonChain(gs, tile);
		VERIFY_HALE_STATUS(err_code, "Flood fill failed");
	}
	
	return err_code;
}


//Handles playing a tile and the associated stuff (mergers, etc.)
static HALE_status_t handleTilePlayPhase(GameState_t* gs)
{
	CHECK_NULL_PTR(gs, "gs");
	
	HALE_status_t err_code = HALE_OK;
	
	//The player should ALWAYS have a valid tile to play when this is called
	//getTileToPlay should ensure that
	uint8_t tile;
	err_code = getTileToPlay(gs, &tile);
	VERIFY_HALE_STATUS(err_code, "Should be impossible: no valid tile to play");
	LOG_MSG_INT("Playing tile", tile);
	
	//Even so, make sure it's a valid move in the first place
	if(!isValidTilePlay(gs, tile))
	{
		PRINT_MSG_INT("Bad tile play requested", tile);
		return HALE_BAD_INPUT;
	}
	
	
	//Get some information about what playing this tile will do
	uint8_t numMergingChains = 0;
	chain_t mergingChains[NUM_CHAINS] = {};
	uint8_t merger = wouldCauseMerger(gs, tile, &numMergingChains, mergingChains);
	uint8_t create = wouldCreateChain(gs, tile);
	
#ifndef GO_FAST_AND_BREAK_THINGS
	//Should never have a merge AND create a chain at the same time
	if(merger && create)
	{
		PRINT_MSG("ERROR: Both merger and creating a chain?!?!?");
		return HALE_SHOULD_BE_IMPOSSIBLE;
	}
#endif
	
	//Each of these functions will do everything required- handling shares,
	//money, updating the board, ...
	if(merger)
	{
		LOG_MSG_INT("Tile would cause merger", tile);
		err_code = handleTilePlayMerger(gs, tile, numMergingChains, mergingChains);
	}
	else if(create)
	{
		LOG_MSG_INT("Tile would create new chain", tile);
		err_code = handleTilePlayCreate(gs, tile);
	}
	else
	{
		err_code = handleTilePlayRegular(gs, tile);
	}
	
	return err_code;
}


//Deals with purchasing shares
static HALE_status_t handleSharePurchasePhase(GameState_t* gs)
{
	//This should only be called from runGame(), so this should NEVER
	//happen. Thus being a fatal error.
	CHECK_NULL_PTR_FATAL(gs, "gs");
	
	//Check if gs->currentPlayer is in bounds
#ifndef GO_FAST_AND_BREAK_THINGS
	if(gs->currentPlayer >= gs->numPlayers)
	{
		PRINT_MSG_INT("gs->currentPlayer is invalid", gs->currentPlayer);
		HANDLE_UNRECOVERABLE_ERROR(HALE_OOB);
	}
#endif //GO_FAST_AND_BREAK_THINGS

	HALE_status_t err_code = HALE_OK;

	uint8_t currentPlayer = gs->currentPlayer;

	GameState_t gsSanitized;
	err_code = makeSanitizedGameStateCopy(&gsSanitized, gs, currentPlayer);
	VERIFY_HALE_STATUS(err_code, "Couldn't make sanitized game state for requesting share purchase");

	//Allow player to request a purchase
	uint8_t buyRequest[NUM_CHAINS] = {0};
	gsSanitized.players[currentPlayer].actions.buyStock(&gsSanitized, currentPlayer, buyRequest);
	
	//Validate that the purchase is valid:
	//-the request is for no more than 3 shares (==SHARES_PER_TURN) total
	//-the chains exist
	//-there are enough shares available to fulfill the request
	//-the player has enough money
	
	uint8_t requestIsValid = 1;
	
	
	//Ensure request is for no more than 3 (==SHARES_PER_TURN) shares total
	int totalSharesRequested = 0;
	for(int i = 0; i < NUM_CHAINS; i++)
	{
		totalSharesRequested += buyRequest[i];
	}
	if(totalSharesRequested > SHARES_PER_TURN)
	{
		// PRINT_MSG_INT("Too many shares requested", totalSharesRequested);
		requestIsValid = 0;
	}
	
	
	//Ensure all requested shares have existing chains associated with them
	uint8_t chainSizes[NUM_CHAINS];
	err_code = getChainSizes(gs, chainSizes);
	VERIFY_HALE_STATUS(err_code, "Couldn't get chain sizes");
	for(int i = 0; i < NUM_CHAINS; i++)
	{
		if(buyRequest[i] && chainSizes[i] < 2)
		{
			// PRINT_MSG_INT("Chain doesn't exist", i);
			requestIsValid = 0;
		}
	}
	
	
	//Ensure there are enough shares to fulfill the request
	for(int i = 0; i < NUM_CHAINS; i++)
	{
		if(buyRequest[i] > gs->remainingStocks[i])
		{
			// PRINT_MSG_INT("Not enough shares remining of chain", i);
			requestIsValid = 0;
		}
	}
	
	
	//Ensure player has enough money to pay for the requested purchase
	int totalCost = 0;
	int32_t pricePerShare[NUM_CHAINS];
	err_code = getChainPricesPerShare(gs, pricePerShare, NULL);
	VERIFY_HALE_STATUS(err_code, "Couldn't get pricePerShare");
	
	for(int i = 0; i < NUM_CHAINS; i++)
	{
		totalCost += pricePerShare[i] * buyRequest[i];
	}
	
	if(totalCost > gs->players[currentPlayer].cash)
	{
		// PRINT_MSG_INT("Purchase request cost exceeds player funds", totalCost);
		requestIsValid = 0;
	}
	
	
	//If the request is valid, execute the trades
	if(requestIsValid)
	{
		gs->players[currentPlayer].cash -= totalCost;
		//PRINT_MSG("FIXME: Should probably have a generic function for modifying shares a player has, handling moving them between the pool and the player...");
		for(int i = 0; i < NUM_CHAINS; i++)
		{
			if(buyRequest[i] > 0)
			{
				LOG_MSG_INT(chainNames[i], buyRequest[i]);
			}
			gs->players[currentPlayer].stocks[i] += buyRequest[i];
			gs->remainingStocks[i] -= buyRequest[i];
		}
	}
	//Otherwise... well, too bad. The penalty for not checking the validity
	//of your trade ahead of time is that you don't get to trade
	else
	{
		// PRINT_MSG_INT("Invalid trade request; ignoring trade phase. Player", currentPlayer);
	}
	
	return err_code;
}


static HALE_status_t handleEndGameQueryPhase(GameState_t* gs, uint8_t* endGame)
{
	CHECK_NULL_PTR(gs, "gs");
	CHECK_NULL_PTR(endGame, "endGame");
	
	GameState_t gsSanitized;
	HALE_status_t err = makeSanitizedGameStateCopy(&gsSanitized, gs, gs->currentPlayer);
	VERIFY_HALE_STATUS(err, "Couldn't get sanitized game state");
	
	uint8_t wantToEndGame = gsSanitized.players[gs->currentPlayer].actions.endGame(&gsSanitized, gs->currentPlayer);
	
	//Could/should probably just assign this directly, but want to keep all player actions completely
	//separate from game
	if(wantToEndGame)
	{
		*endGame = 1;
	}
	
	return HALE_OK;
}


void runGame(uint8_t numPlayers)
{
	//int i;
	HALE_status_t err_code = HALE_OK;
	
	//Keep the game state internal to the function; don't want to let
	//anything else screw with it (except for all the stuff we pass it to)
	GameState_t gs;
	//HALE_status_t err_code = HALE_OK;
	
	//Zero out the board
	initializeGameState(&gs);
	
	//Configure players (populate actions)
	err_code = configurePlayers(&gs, numPlayers);
	VERIFY_HALE_STATUS_FATAL(err_code, "Failed to configure players");
	
	//Deal initial tiles
	err_code = dealInitialTiles(&gs);
	VERIFY_HALE_STATUS_FATAL(err_code, "Failed to deal initial tiles");
	
	//Randomize first player
	gs.currentPlayer = getRandom8(0, gs.numPlayers-1);
	LOG_MSG_INT("Starting Player", gs.currentPlayer);
	//Place <numPlayers> random tiles on the board
	err_code = placeInitialTiles(&gs);
	printGameBoard(&gs);
	for(int i = 0; i < gs.numPlayers; i++)
	{
		printPlayer(&gs, i);
	}
	VERIFY_HALE_STATUS_FATAL(err_code, "Failed to deal place tiles");
	
	//So now everybody has tiles, the board is set up... time to start!
	uint8_t gameOver = 0;
	while(!gameOver)
	{
		LOG_PRINT("\n\nTURN START\n");
		LOG_MSG_INT("Player Number", gs.currentPlayer);
		//Make sure the current player can actually play... If not, try
		//to do something about it.
		uint8_t numValidTiles = getNumValidTiles(&gs, gs.currentPlayer);
		while(numValidTiles == 0)
		{
			PRINT_MSG_INT("Player has no valid tiles, redealing. Player", gs.currentPlayer);
			
			//Don't try to redeal tiles if there aren't any left
			if(gs.remainingTiles > 0)
			{
				HALE_status_t err = redealTiles(&gs, gs.currentPlayer);
				
				//Running out of tiles is acceptable (could have partially redealt)
				//Anything else... not so much
				if(err != HALE_OK && err != HALE_TILE_POOL_EMPTY)
				{
					PRINT_MSG("FATAL: Couldn't handle redealing player hand");
					HANDLE_UNRECOVERABLE_ERROR(err);
				}
			}
			//If the player has no valid tiles AND there are no tiles to redeal with... tough. No playing for you!
			else
			{
				PRINT_MSG_INT("No tiles left to redeal ot player", gs.currentPlayer);
				break;
			}
			
			//Hopefully have at least one valid tile now...
			numValidTiles = getNumValidTiles(&gs, gs.currentPlayer);
		}
		
		//Request a tile to play from the current player
		//If the player has no valid tiles at this point, it must mean the
		//draw pile is out as well, and they have to skip playing
		if(numValidTiles > 0)
		{
			err_code = handleTilePlayPhase(&gs);
			VERIFY_HALE_STATUS_FATAL(err_code, "handleTilePlayPhase failed");
		}
		//If we can't play, at least make a note of it
		else
		{
			LOG_MSG_INT("Playing tile", TILE_NULL);
			PRINT_MSG_INT("No tiles left, and player can't play; skipping tile play phase", gs.currentPlayer);
#ifdef ENABLE_PARANOID_CHECKS
			//This should never happen- should only be possible to end up with
			//an unplayable hand if the tile draw stack is empty
			if(gs.remainingTiles > 0)
			{
				PRINT_MSG("FATAL: Player can't play, yet tiles remain in draw pile");
				HANDLE_UNRECOVERABLE_ERROR(HALE_SHOULD_BE_IMPOSSIBLE);
			}
#endif
		}
		
		
		//Allow player to purchase shares
		err_code = handleSharePurchasePhase(&gs);
		VERIFY_HALE_STATUS_FATAL(err_code, "handleSharePurchasePhase failed");
		
		
		if(canEndGame(&gs))
		{
			//PRINT_MSG("Ending game is allowed right now!");
			err_code = handleEndGameQueryPhase(&gs, &gameOver);
			VERIFY_HALE_STATUS_FATAL(err_code, "handleEndGameQueryPhase failed");
			
			//No need to deal tiles or set next player if we're ending the game
			if(gameOver)
			{
				break;
			}
		}
		
		
		//Deal a tile to the player that just went, if there are tiles left
		if(gs.remainingTiles)
		{
			err_code = dealTile(&gs, gs.currentPlayer);
			VERIFY_HALE_STATUS_FATAL(err_code, "dealTile failed");
		}
		
		//Set the next player
		gs.currentPlayer = (gs.currentPlayer + 1) % gs.numPlayers;
		
		//FIXME: Testing
		printGameBoard(&gs);
		for(int i = 0; i < gs.numPlayers; i++)
		{
			printPlayer(&gs, i);
		}
		//return;
		
#ifdef TURN_DELAY
		usleep(TURN_DELAY*1000);
#endif
		
	} //while !gameover
	// for consistency with printing
	printGameBoard(&gs);
	for(int i = 0; i < gs.numPlayers; i++)
	{
		printPlayer(&gs, i);
	}

	//Now that the game is over, display relevant statistics:
	//-winner
	//-holdings of each player (each stock type, cash)
	//-total value of each player (bonuses + holdings + cash)
	//-Player type/name
	
	LOG_MSG("END OF GAME");
	// PRINT_MSG("FIXME: handle ties!");
	uint8_t winner = 0;
	int32_t winningValue = 0;
	for(int i = 0; i < gs.numPlayers; i++)
	{
		int32_t tmpVal;
		calculatePlayerValue(&gs, i, &tmpVal);
		if(tmpVal > winningValue)
		{
			winner = i;
			winningValue = tmpVal;
		}
	}
	
	LOG_MSG_INT("Winner is player", winner);
	LOG_MSG_INT("Winning value", winningValue);
	
	printGameBoard(&gs);
	for(int i = 0; i < gs.numPlayers; i++)
	{
		printPlayer(&gs, i);
	}
}
