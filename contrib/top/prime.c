/*
 * Prime number generator.  It prints on stdout the next prime number
 * higher than the number specified as argv[1].
 */

#include <math.h>

main(argc, argv)

int argc;
char *argv[];

{
    double i, j;
    int f;

    if (argc < 2)
    {
	exit(1);
    }

    i = atoi(argv[1]);
    while (i++)
    {
	f=1;
	for (j=2; j<i; j++)
	{
	    if ((i/j)==floor(i/j))
	    {
		f=0;
		break;
	    }
	}
	if (f)
	{
	    printf("%.0f\n", i);
	    exit(0);
	}
    }
}
