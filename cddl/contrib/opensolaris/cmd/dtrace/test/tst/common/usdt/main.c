#include <stdlib.h>
#include <sys/sdt.h>
#include "prov.h"

int
main(int argc, char **argv, char **envp)
{
	envp[0] = (char*)0xff;
	TESTER_ENTRY();
	return 0;
}
