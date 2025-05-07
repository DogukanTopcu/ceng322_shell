/*
 * test.c
 * 
 * Purpose:
 * This file is created as a minimal C program to test the functionality
 * of executing system commands in the custom shell (shell322).
 *
 * Specifically, it is used to verify:
 * 1. That the shell can compile a C file using `gcc test.c`
 * 2. That the shell can execute the resulting binary using `./a.out`
 * 3. That the logical AND operator (`&&`) correctly chains these commands
 *    as in `gcc test.c && ./a.out`
 *
 * The program simply prints a message to confirm it ran successfully.
 */
#include <stdio.h>

int main() {
    printf("✅ Hello from test.c! The program ran successfully.\n");
    return 0;
}