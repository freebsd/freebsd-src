#include <stdio.h>

#define MAXALLOCS   100
#define INC	    50

extern char *sbrk();
extern char *malloc();

main()
{
    char *ptr1, *ptr2;
    char *obrk, *nbrk;
    int i;
    int small = 10;
    int large = 128;

    obrk = sbrk(0);
    printf("break is initially 0x%x\n", obrk);

    ptr1 = malloc(small);
    ptr2 = malloc(small);
    for(i = 0; i < MAXALLOCS; i++) {
	(void) malloc(small);
	free(ptr1);
	ptr1 = malloc(large);
	large += INC;
	(void) malloc(small);
	free(ptr2);
	ptr2 = malloc(large);
	large += INC;
	mal_heapdump(stdout);
    }
    nbrk = sbrk(0);
    printf("break is 0x%x (%d bytes sbrked)\n",
     nbrk, nbrk - obrk);
    exit(0);
}
