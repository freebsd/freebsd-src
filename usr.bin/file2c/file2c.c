/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dknet.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $FreeBSD$
 *
 */

#include <stdio.h>

int
main(int argc, char **argv)
{
    int i,j,k;
    char s[10];

    if (argc > 1)
        printf("%s\n",argv[1]);
    k = 0;
    j = 0;
    while((i = getchar()) != EOF) {
	if(k++) {
	    putchar(',');
	    j++;
	}
	if (j > 70) {
	    putchar('\n');
	    j = 0;
	}
	printf("%d",i);
	if (i > 99)
	    j += 3;
	else if (i > 9)
	    j += 2;
	else
	    j++;
    }
    putchar('\n');
    if (argc > 2)
        printf("%s\n",argv[2]);
    return 0;
}
