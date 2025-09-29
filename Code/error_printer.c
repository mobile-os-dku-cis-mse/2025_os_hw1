// error_printer.c
#include <stdio.h>
#include "error_printer.h"

// Print error
void print_error(const char *msg){
    fprintf(stderr, "Error: %s\n", msg);
}