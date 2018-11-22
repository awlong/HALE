#ifndef HALE_CONFIG_H
#define HALE_CONFIG_H

#include <inttypes.h>


//Optionally enable paranoid checks, at the expense of some speed
//#define ENABLE_PARANOID_CHECKS

//Optionally turn off some safety checks to try to go faster
//#define GO_FAST_AND_BREAK_THINGS

//Artificial delay between turns, for debugging
//Milliseconds between turns. Comment out to disable (or set to 0, but
//that still has overhead)
//#define TURN_DELAY 100

//Uncomment to enable color printing of the game board
//Annoying if redirecting to a file, but nice if looking on a terminal
//#define ENABLE_COLOR_BOARD_PRINT

//Uncomment to print tile numbers when board is printed
//Useful for humans only
//#define ENABLE_BOARD_TILE_NUMBERS

//Uncomment to enable Python bindings
//#define HALE_ENABLE_PYTHON


//Return values
typedef enum	{	HALE_OK = 0,
					HALE_FAIL,
					HALE_TILE_POOL_EMPTY,
					HALE_TILE_POOL_CORRUPTED,
					HALE_PLAYER_HAND_FULL,
					HALE_OOB,
					HALE_BAD_INPUT,
					HALE_FUNC_NOT_IMPLEMENTED,
					HALE_AI_BADSTATE,
					HALE_SHOULD_BE_IMPOSSIBLE,
					HALE_BOARD_CORRUPTED,
					HALE_NUM_STATUS
				} HALE_status_t;

extern const char* err_msg[];


//How much money each player starts with
#define STARTING_CASH (6000)

//How many tiles are in a player's hand
#define HAND_SIZE (6)

//How many units of stock are available for each chain
#define NUM_STOCKS (25)

//Maximum number of players
#define MAX_PLAYERS (4)



#define BOARD_WIDTH (12)
#define BOARD_HEIGHT (9)

//Shortcut for when you need the total size of the board
#define BOARD_TILES (BOARD_WIDTH * BOARD_HEIGHT)

//Specifies an invalid board index (eg, left of the left column)
#define BOARD_NULL (0xff)

//Null tile; represents either an empty place on the board or an emptry
//slot in a player's hand
#define TILE_NULL (0xff)


//How many shares you can buy per turn
#define SHARES_PER_TURN (3)



#endif
