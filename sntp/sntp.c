#include <config.h>

#include "main.h"

volatile int debug;

int 
main (
	int argc,
	char **argv
	) 
{
	return sntp_main(argc, argv);
}
