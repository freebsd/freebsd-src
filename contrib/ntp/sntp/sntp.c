#include <config.h>

#include "main.h"

const char * progname;

int 
main (
	int	argc,
	char **	argv
	) 
{
	return sntp_main(argc, argv, Version);
}
