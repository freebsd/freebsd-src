/*
 * posted to the net by someone who asked "Why is this causing malloc to
 * dump core!  Modified slightly to free the pointers, which causes my
 * debugging malloc to find the bug.  Turning on malloc_debug(2) also
 * spots the problem.
 */
#include <stdio.h>

int
main()
{
        char *p[3], wd[128];
        int  len, i;
        char *malloc();
        int      strlen();

        strcpy(wd,"test");

        for (i=0; i<3; i++) {
                len = strlen(wd);
                if ((p[i] = malloc(len)) == NULL) {
                        printf("ERROR: malloc failed\n");
                        exit(-1);
                }
                else
                        strcpy(p[i],wd);
        }
	for(i=0; i < 3; i++)
		free(p[i]);
	return 0;
}
