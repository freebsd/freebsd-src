#include <stdio.h>
#include <stdlib.h>

typedef int (*main_t)(int, char **);

#define DECL(foo) int foo(int, char**);
DECL(dumpcis_main);
DECL(enabler_main);
DECL(help_main);
DECL(pccardmem_main);
DECL(rdmap_main);
DECL(rdreg_main);
DECL(wrattr_main);
DECL(wrreg_main);

struct {
	char   *name;
	main_t  func;
	char   *help;
} subcommands[] = {
	{ "dumpcis", dumpcis_main, "Prints CIS for all cards" },
	{ "enabler", enabler_main, "Device driver enabler" },
	{ "help", help_main, "Prints command summary" },
	{ "pccardmem", pccardmem_main, "Allocate memory for pccard driver" },
	{ "rdmap", rdmap_main, "Read pcic mappings" },
	{ "rdreg", rdreg_main, "Read pcic register" },
	{ "wrattr", wrattr_main, "Write byte to attribute memory" },
	{ "wrreg", wrreg_main, "Write pcic register" },
	{ 0, 0 }
};

int
main(int argc, char **argv)
{
	int     i;

	for (i = 0; argc > 1 && subcommands[i].name; i++) {
		if (!strcmp(argv[1], subcommands[i].name)) {
			argv[1] = argv[0];
			return (*subcommands[i].func) (argc - 1, argv + 1);
		}
	}
	if (argc > 1)
		fprintf(stderr, "Unknown Subcommand.\n");
	return help_main(argc, argv);
}

int
help_main(int argc, char **argv)
{
	int     i;

	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "\t%s <subcommand> <arg> ...\n", argv[0]);
	fprintf(stderr, "Subcommands:\n");
	for (i = 0; subcommands[i].name; i++)
		fprintf(stderr, "\t%s\n\t\t%s\n",
		    subcommands[i].name, subcommands[i].help);
	return 1;
}
