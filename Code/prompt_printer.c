// prompt_printer.c
#include <stdlib.h>
#include <stdio.h>
#include "prompt_printer.h"

// Load environment variable USER
static const char* load_user(){
    return getenv("USER");
}

// Load environment variable PWD
static const char* load_pwd(){
    return getenv("PWD");
}

// Print prompt
void print_prompt(){
    printf("%s %s \n$ ", load_user(), load_pwd());
    fflush(stdout);
}