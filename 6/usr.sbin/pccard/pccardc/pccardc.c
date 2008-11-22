/*
 * Copyright (c) 1995 Andrew McRae.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int (*main_t)(int, char **);

#define DECL(foo) int foo(int, char**)
DECL(beep_main);
DECL(dumpcis_main);
DECL(dumpcisfile_main);
DECL(enabler_main);
DECL(help_main);
DECL(pccardmem_main);
DECL(power_main);
DECL(rdattr_main);
DECL(rdmap_main);
DECL(rdreg_main);
DECL(wrattr_main);
DECL(wrreg_main);

struct {
	char   *name;
	main_t  func;
	char   *help;
} subcommands[] = {
	{ "beep", beep_main, "Beep type" },
	{ "dumpcis", dumpcis_main, "Prints CIS for all cards" },
	{ "dumpcisfile", dumpcisfile_main, "Prints CIS from a file" },
	{ "enabler", enabler_main, "Device driver enabler" },
	{ "help", help_main, "Prints command summary" },
	{ "pccardmem", pccardmem_main, "Allocate memory for pccard driver" },
	{ "power", power_main, "Power on/off slots" },
	{ "rdattr", rdattr_main, "Read attribute memory" },
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
		warnx("unknown subcommand");
	return help_main(argc, argv);
}

int
help_main(int argc, char **argv)
{
	int     i;

	fprintf(stderr, "usage: pccardc <subcommand> <arg> ...\n");
	fprintf(stderr, "subcommands:\n");
	for (i = 0; subcommands[i].name; i++)
		fprintf(stderr, "\t%s\n\t\t%s\n",
		    subcommands[i].name, subcommands[i].help);
	return 1;
}
