#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "state.h"
#include "chain.h"
#include "util.h"

#include "config.h"
#ifdef HALE_ENABLE_PYTHON
#include <Python.h>
#endif //HALE_ENABLE_PYTHON

int parse_args(int argc, char* argv[])
{
	char* logname = NULL;
	int code;
	while((code = getopt(argc, argv, "l:")) != -1)
	{
		switch(code)
		{
			case 'l':
				logname = optarg;
				break;
			case '?':
				if(optopt == 'l')
					fprintf(stderr, "Option -%c requires an argument (logfile to store to). \n", optopt);
				else
					fprintf(stderr, "Unknown option '-%c' .\n", optopt);
				return 1;
			default:
				abort();
		}
	}
	if(logname)
	{
		if(log_ptr != NULL)
			fclose(log_ptr);
		log_ptr = fopen(logname, "w");
	}
	if (log_ptr == NULL)
		log_ptr = stdout;
	return 0;
}
int main(int argc, char* argv[])
{
	//FIXME: Figure out player configuration (number of players, human/AI)
	// PRINT_MSG("FIXME: Need to identify player configuration: number of players, human/AI, ...");
	
	//FIXME: Initialize UI
	
	//Initialize Python, if appropriate
	if(parse_args(argc, argv))
	{
		fprintf(stderr, "Aborting...\n");
		return 0;
	}
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

	if(log_ptr != stdout)
		fclose(log_ptr);
	return 0;
}
