#include <time.h>
#include <stdlib.h>
#include <stdio.h>

#include "state.h"
#include "chain.h"
#include "util.h"

#include "config.h"
#ifdef HALE_ENABLE_PYTHON
#include <Python.h>
#endif //HALE_ENABLE_PYTHON

int main(int argc, char* argv[])
{
	//FIXME: Figure out player configuration (number of players, human/AI)
	// PRINT_MSG("FIXME: Need to identify player configuration: number of players, human/AI, ...");
	
	//FIXME: Initialize UI
	
	//Initialize Python, if appropriate
#ifdef HALE_ENABLE_PYTHON
	Py_Initialize();
	//Hack to load modules from current directory
	PyRun_SimpleString(
	"import sys\n"
	"import os\n"
	"sys.path.append(os.getcwd())\n" );
#endif //HALE_ENABLE_PYTHON
	
	//Seed random number generator
	srand(time(NULL));
	
	//Run the actual game
	//FIXME: Need to actually use the number of players...
	// PRINT_MSG("FIXME: Need to pass real number of players to runGame(), not a constant 4");
	runGame(4);
	return 0;
}
