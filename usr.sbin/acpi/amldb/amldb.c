/*-
 * Copyright (c) 1999 Mitsuru IWASAKI <iwasaki@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id: amldb.c,v 1.8 2000/08/08 14:12:24 iwasaki Exp $
 *	$FreeBSD$
 */

#include <sys/param.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <dev/acpi/aml/aml_amlmem.h>
#include <dev/acpi/aml/aml_common.h>
#include <dev/acpi/aml/aml_env.h>
#include <dev/acpi/aml/aml_parse.h>
#include <dev/acpi/aml/aml_region.h>

#include <assert.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "debug.h"

int	regdump_enabled = 0;
int	memstat_enabled = 0;
int	showtree_enabled = 0;

static void     aml_init_namespace();

void
aml_init_namespace()
{
	struct	aml_environ env;
	struct	aml_name *newname;

	aml_new_name_group(AML_NAME_GROUP_OS_DEFINED);
	env.curname = aml_get_rootname();
	newname = aml_create_name(&env, "\\_OS_");
	newname->property = aml_alloc_object(aml_t_string, NULL);
	newname->property->str.needfree = 0;
	newname->property->str.string = "Microsoft Windows NT";
}

static int
load_dsdt(const char *dsdtfile)
{
	struct aml_environ	env;
	u_int8_t		*code;
	struct stat		sb;
	int			fd;

	printf("Loading %s...", dsdtfile);

	fd = open(dsdtfile, O_RDONLY, 0);
	if (fd == -1) {
		perror("open");
		exit(-1);
	}
	if (fstat(fd, &sb) == -1) {
		perror("fstat");
		exit(-1);
	}
	if ((code = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0)) == NULL) {
		perror("mmap");
		exit(-1);
	}
	aml_init_namespace();

	aml_new_name_group((int)code);
	bzero(&env, sizeof(env));

	/*
	 * Microsoft asl.exe generates 0x23 byte additional info.
	 * at the begining of the file, so just ignore it.   
	 */
	if (strncmp(code, "DSDT", 4) == 0) {
		env.dp = code + 0x23;
	} else {
		env.dp = code;
	}
	env.end = code + sb.st_size;
	env.curname = aml_get_rootname();

	aml_local_stack_push(aml_local_stack_create());
	aml_parse_objectlist(&env, 0);
	aml_local_stack_delete(aml_local_stack_pop());

	assert(env.dp == env.end);
	env.dp = code;
	env.end = code + sb.st_size;

	printf("done\n");

	aml_debug = 1;		/* debug print enabled */

	if (showtree_enabled == 1) {
		aml_showtree(env.curname, 0);
	}
	do {
		aml_dbgr(&env, &env);
	} while (env.stat != aml_stat_panic);

	aml_debug = 0;		/* debug print disabled */

	if (regdump_enabled == 1) {
		aml_simulation_regdump("region.dmp");
	}
	while (name_group_list->id != AML_NAME_GROUP_ROOT) {
		aml_delete_name_group(name_group_list);
	}

	if (memstat_enabled == 1) {
		memman_statistics(aml_memman);
	}
	memman_freeall(aml_memman);

	return (0);
}

static void
usage(const char *progname)
{

	printf("usage: %s [-d] [-s] [-t] [-h] dsdt_files...\n", progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	char	c, *progname;
	int	i;

	progname = argv[0];
	while ((c = getopt(argc, argv, "dsth")) != -1) {
		switch (c) {
		case 'd':
			regdump_enabled = 1;
			break;
		case 's':
			memstat_enabled = 1;
			break;
		case 't':
			showtree_enabled = 1;
			break;
		case 'h':
		default:
			usage(progname);
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	if (argc == 0) {
		usage(progname);
	}
	for (i = 0; i < argc; i++) {
		load_dsdt(argv[i]);
	}

	return (0);
}
