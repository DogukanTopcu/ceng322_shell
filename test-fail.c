/*
 * test-fail.c
 *
 * Purpose:
 * This file is intentionally written with a syntax error to test the shell's
 * behavior when a command fails to compile.
 *
 * Specifically, it verifies that:
 * 1. `gcc test-fail.c` fails due to the missing semicolon.
 * 2. The shell correctly prevents the execution of any command after `&&`
 *    because the first command fails.
 *
 * This helps confirm proper implementation of the logical AND operator handling.
 */
#include <stdio.h>
int main() {
    printf("❌ Test failed.")
    return 0;
}