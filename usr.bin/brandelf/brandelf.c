/*-
 * Copyright (c) 2000, 2001 David O'Brien
 * Copyright (c) 1996 Søren Schmidt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/elf_common.h>
#include <sys/elf.h>
#include <sys/endian.h>
#include <sys/errno.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int elftype(const char *);
static const char *iselftype(int);
static void printelftypes(void);
static void usage(void);

struct ELFtypes {
	const char *str;
	int value;
};
/* XXX - any more types? */
static struct ELFtypes elftypes[] = {
	{ "FreeBSD",	ELFOSABI_FREEBSD },
	{ "Linux",	ELFOSABI_LINUX },
	{ "Solaris",	ELFOSABI_SOLARIS },
	{ "SVR4",	ELFOSABI_SYSV }
};

#ifndef EM_MIPS_CHERI128
#define	EM_MIPS_CHERI128	0xC128
#endif
#ifndef EM_MIPS_CHERI256
#define	EM_MIPS_CHERI256	0xC256
#endif

int
main(int argc, char **argv)
{
	const char *strtype = "FreeBSD";
	int type = ELFOSABI_FREEBSD;
	int retval = 0;
	int ch, change = 0, force = 0, listed = 0;
	ssize_t writeout;
	int cheri = 0;

	while ((ch = getopt(argc, argv, "c:f:lt:v")) != -1)
		switch (ch) {
		case 'c':
			if (force)
				errx(1, "c option incompatible with f option");
			if (change)
				errx(1, "c option incompatible with t option");
			cheri = atoi(optarg);
			if (errno == ERANGE || (cheri != 128 && cheri != 256)) {
				warnx("invalid argument to option c: %s",
				    optarg);
				usage();
			}
			break;
		case 'f':
			if (change)
				errx(1, "f option incompatible with t option");
			if (cheri)
				errx(1, "f option incompatible with c option");
			force = 1;
			type = atoi(optarg);
			if (errno == ERANGE || type < 0 || type > 255) {
				warnx("invalid argument to option f: %s",
				    optarg);
				usage();
			}
			break;
		case 'l':
			printelftypes();
			listed = 1;
			break;
		case 'v':
			/* does nothing */
			break;
		case 't':
			if (cheri)
				errx(1, "t option incompatible with c option");
			if (force)
				errx(1, "t option incompatible with f option");
			change = 1;
			strtype = optarg;
			break;
		default:
			usage();
	}
	argc -= optind;
	argv += optind;
	if (!argc) {
		if (listed)
			exit(0);
		else {
			warnx("no file(s) specified");
			usage();
		}
	}

	if (!force && !cheri && (type = elftype(strtype)) == -1) {
		warnx("invalid ELF type '%s'", strtype);
		printelftypes();
		usage();
	}

	while (argc) {
		int e_data, e_machine, fd, target_e_machine;
		union {
			unsigned char	ident[EI_NIDENT];
			Elf32_Ehdr	ehdr32;
			Elf64_Ehdr	ehdr64;
		} buffer;

		if ((fd = open(argv[0], change || cheri || force ? O_RDWR : O_RDONLY,
		    0)) < 0) {
			warn("error opening file %s", argv[0]);
			retval = 1;
			goto fail;
		}
		if (read(fd, &buffer, EI_NIDENT) < EI_NIDENT) {
			warnx("file '%s' too short", argv[0]);
			retval = 1;
			goto fail;
		}
		if (buffer.ident[0] != ELFMAG0 || buffer.ident[1] != ELFMAG1 ||
		    buffer.ident[2] != ELFMAG2 || buffer.ident[3] != ELFMAG3) {
			warnx("file '%s' is not ELF format", argv[0]);
			retval = 1;
			goto fail;
		}
		if (!change && !cheri && !force) {
			writeout = 0;
			fprintf(stdout,
				"File '%s' is of brand '%s' (%u).\n",
				argv[0], iselftype(buffer.ident[EI_OSABI]),
				buffer.ident[EI_OSABI]);
			if (!iselftype(type)) {
				warnx("ELF ABI Brand '%u' is unknown",
				      type);
				printelftypes();
			}
		} else if (cheri) {
			e_data = buffer.ident[EI_DATA];
			if (e_data != 1 && e_data != 2) {
				warnx("file '%s' has unknown endianness",
				    argv[0]);
				retval = 1;
				goto fail;
			}
			switch (buffer.ident[EI_CLASS]) {
			case ELFCLASS32:
				writeout = sizeof(buffer.ehdr32);
				if (pread(fd, &buffer, writeout, 0) <
				    writeout) {
					warnx("file '%s' too short", argv[0]);
					retval = 1;
					goto fail;
				}
				/* No CHERI on 32-bit architectures */
				warnx("file '%s' not supported by cheri",
				    argv[0]);
				retval = 1;
				goto fail;
				break;

			case ELFCLASS64:
				writeout = sizeof(buffer.ehdr64);
				if (pread(fd, &buffer, writeout, 0) <
				    writeout) {
					warnx("file '%s' too short", argv[0]);
					retval = 1;
					goto fail;
				}
				e_machine = e_data == 1 ?
				    le16toh(buffer.ehdr64.e_machine) :
				    be16toh(buffer.ehdr64.e_machine);
				target_e_machine = cheri == 128 ?
				    EM_MIPS_CHERI128 : EM_MIPS_CHERI256;
				if (e_machine == target_e_machine) {
					break;
				} else if (e_machine == EM_MIPS) {
					buffer.ehdr64.e_machine =
					    e_data == 1 ?
					    htole16(target_e_machine) :
					    htobe16(target_e_machine);
				} else {
					warnx("file '%s' not a CHERI platform",
					    argv[0]);
					retval = 1;
					goto fail;
				}
				break;

			default:
				warnx("file '%s' is an unknown elf class %c",
				   argv[0], buffer.ident[EI_CLASS]);
				retval = 1;
				goto fail;
			}
		} else {
			writeout = EI_NIDENT;
			buffer.ident[EI_OSABI] = type;
		}
		if (pwrite(fd, &buffer, writeout, 0) != writeout) {
			warn("error writing %s %d", argv[0], fd);
			retval = 1;
			goto fail;
		}
fail:
		close(fd);
		argc--;
		argv++;
	}

	return retval;
}

static void
usage(void)
{
	(void)fprintf(stderr,
	    "usage: brandelf [-lv] [-c CHERI_capability_size] file ...\n"
	    "       brandelf [-lv] [-f ELF_ABI_number] file ...\n"
	    "       brandelf [-lv] [-t string] file ...\n");
	exit(1);
}

static const char *
iselftype(int etype)
{
	size_t elfwalk;

	for (elfwalk = 0;
	     elfwalk < sizeof(elftypes)/sizeof(elftypes[0]);
	     elfwalk++)
		if (etype == elftypes[elfwalk].value)
			return elftypes[elfwalk].str;
	return 0;
}

static int
elftype(const char *elfstrtype)
{
	size_t elfwalk;

	for (elfwalk = 0;
	     elfwalk < sizeof(elftypes)/sizeof(elftypes[0]);
	     elfwalk++)
		if (strcasecmp(elfstrtype, elftypes[elfwalk].str) == 0)
			return elftypes[elfwalk].value;
	return -1;
}

static void
printelftypes(void)
{
	size_t elfwalk;

	fprintf(stderr, "known ELF types are: ");
	for (elfwalk = 0;
	     elfwalk < sizeof(elftypes)/sizeof(elftypes[0]);
	     elfwalk++)
		fprintf(stderr, "%s(%u) ", elftypes[elfwalk].str, 
			elftypes[elfwalk].value);
	fprintf(stderr, "\n");
}
