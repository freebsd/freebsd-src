#include <stdio.h>

#define T_INIT	0100
#define T_STOP	0111

long	charin;			/* number of input character */

main(argc, argv)
char	**argv;
{

	int	 npages = 0;
	register int	c;

	while((c=getchar()) != EOF) {
		charin++;
		c &= 0377;
		if(c != T_INIT)
			continue;
		else {
			c=getchar();
			c &= 0377;
			if(c == T_STOP) {
				npages++;
				charin++;
			}
		}
	}
	if(charin<5) {
		fprintf(stderr, "%s: no input\n", argv[0]);
		exit(1);
	}
	printf("%d\n", npages);
}
