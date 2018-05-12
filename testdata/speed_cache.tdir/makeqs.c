/**
 * \file
 * This file creates queries for the speed test
 */
#include <stdio.h>
#include <stdlib.h>
/** main program to create queries, pass the number of them. */
int main(int argc, char** argv) {
        int i;
        int max = atoi(argv[1]);
        for(i=0; i<max; i++)
                printf("a%8.8d.example.com IN A\n", i);
        return 0;
}
