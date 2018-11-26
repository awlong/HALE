#ifndef HALE_UTIL_H
#define HALE_UTIL_H

#include <inttypes.h>
#include <stdio.h>
// #include <stdlib.h>

#include "config.h"

extern FILE* log_ptr;

#define HANDLE_UNRECOVERABLE_ERROR(x) handleUnrecoverableError(__func__, x)

#define LOG_PRINT(format, ...) if (log_ptr) {fprintf(log_ptr, format, ##__VA_ARGS__);}
#define LOG_MSG(msg) if(log_ptr) {fprintf(log_ptr, "%s: %s\n", __func__, msg);}
#define LOG_MSG_ARG(msg ,arg) if(log_ptr) {fprintf(log_ptr, "%s: %s: %s\n", __func__, msg, arg);}
#define LOG_MSG_INT(msg, arg) if(log_ptr) {fprintf(log_ptr, "%s: %s: %d\n", __func__, msg, arg);}
#define LOG_MSG_LONG(msg, arg) if(log_ptr) {fprintf(log_ptr, "%s: %s: %ld\n", __func__, msg, arg);}
#define LOG_MSG_CONLINE(msg, arg)
#define PRINT_MSG(msg) fprintf(stdout, "%s: %s\n", __func__, msg)
#define PRINT_MSG_ARG(msg,arg) fprintf(stdout, "%s: %s: %s\n", __func__, msg, arg)
#define PRINT_MSG_INT(msg,arg) fprintf(stdout, "%s: %s: %d\n", __func__, msg, arg)
#define PRINT_MSG_LONG(msg,arg) fprintf(stdout, "%s: %s: %ld\n", __func__, msg, arg)

#ifdef ENABLE_COLOR_BOARD_PRINT
#define ANSI_COLOR_RED	 "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE	"\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN	"\x1b[36m"
#define ANSI_COLOR_BRIGHT_RED	 "\x1b[31;1m"
#define ANSI_COLOR_BRIGHT_GREEN   "\x1b[32;1m"
#define ANSI_COLOR_BRIGHT_YELLOW  "\x1b[33;1m"
#define ANSI_COLOR_BRIGHT_BLUE	"\x1b[34;1m"
#define ANSI_COLOR_BRIGHT_MAGENTA "\x1b[35;1m"
#define ANSI_COLOR_BRIGHT_CYAN	"\x1b[36;1m"
#define ANSI_COLOR_RESET   "\x1b[0m"
#else
#define ANSI_COLOR_RED ""
#define ANSI_COLOR_GREEN ""
#define ANSI_COLOR_YELLOW ""
#define ANSI_COLOR_BLUE ""
#define ANSI_COLOR_MAGENTA ""
#define ANSI_COLOR_CYAN ""
#define ANSI_COLOR_BRIGHT_RED ""
#define ANSI_COLOR_BRIGHT_GREEN ""
#define ANSI_COLOR_BRIGHT_YELLOW ""
#define ANSI_COLOR_BRIGHT_BLUE ""
#define ANSI_COLOR_BRIGHT_MAGENTA ""
#define ANSI_COLOR_BRIGHT_CYAN ""
#define ANSI_COLOR_RESET ""
#endif

//We'll check for NULL pointers... unless we want to go fast and break things
#ifndef GO_FAST_AND_BREAK_THINGS
#define CHECK_NULL_PTR_FATAL(ptr,var) {if(ptr == NULL){PRINT_MSG_ARG("FATAL: NULL ptr: ", var); HANDLE_UNRECOVERABLE_ERROR(HALE_BAD_INPUT);}}
#else
#define CHECK_NULL_PTR_FATAL(ptr,var) {}
#endif

//We'll check for NULL pointers... unless we want to go fast and break things
#ifndef GO_FAST_AND_BREAK_THINGS
#define CHECK_NULL_PTR(ptr,var) {if(ptr == NULL){PRINT_MSG_ARG("NULL ptr: ", var);  return HALE_BAD_INPUT;}}
#else
#define CHECK_NULL_PTR(ptr,var) {}
#endif

//Check if a HALE_status_t is OK; return it if not
#ifndef VERIFY_HALE_STATUS
#define VERIFY_HALE_STATUS(status,msg) {if(status != HALE_OK){PRINT_MSG(msg); return status;}}
#endif


void handleUnrecoverableError(const char* func, HALE_status_t err_code);

//Generates a random number between the provided min and max values
//This could be implementation-dependent, so bring out into its own function
uint8_t getRandom8(uint16_t min, uint16_t max);

#endif
