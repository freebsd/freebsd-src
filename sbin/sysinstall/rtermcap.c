#include <stdio.h>
#include <termcap.h>

int
main(int argc, char **argv)
{
	char	buf[4096];
	int	i;

	i = tgetent(buf, argv[1]);
	
	printf("%s",buf);
	return 0;
}
