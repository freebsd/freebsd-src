#include <stdio.h>

#define MAXALLOCS   1000
#define SIZE	    50

extern char *sbrk();
extern char *malloc();

main()
{
    char *ptr[MAXALLOCS];
    char *obrk, *nbrk;
    int i;

    obrk = sbrk(0);
    printf("break is initially 0x%x\n", obrk);
    
    for(i = 0; i < MAXALLOCS; i++) {
	ptr[i] = malloc(SIZE);
    }
    nbrk = sbrk(0);
    printf("break is 0x%x (%d bytes sbrked) after %d allocations of %d\n",
     nbrk, nbrk - obrk, MAXALLOCS, SIZE);
    for(i = 0; i < MAXALLOCS; i++) {
	free(ptr[i]);
    }
    nbrk = sbrk(0);
    printf("break is 0x%x (%d bytes sbrked) after freeing all allocations\n",
     nbrk, nbrk - obrk);
    fflush(stdout);

    /* Should be enough memory for this without needing to sbrk */
    (void) malloc(SIZE * (MAXALLOCS / 2));
    nbrk = sbrk(0);
    
    printf("break is 0x%x (%d bytes sbrked) after allocating %d\n",
     nbrk, nbrk - obrk, SIZE * (MAXALLOCS/2));

    exit(0);
}
