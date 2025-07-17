/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 The FreeBSD Foundation.
 *
 * This software was developed by Bora Ozarslan under sponsorship from
 * the FreeBSD Foundation.
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
 */

#include <sys/param.h>
#include <sys/elf_common.h>
#include <sys/endian.h>
#include <sys/stat.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <gelf.h>
#include <getopt.h>
#include <libelf.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "_elftc.h"
static bool convert_to_feature_val(const char *, uint32_t *);
static bool edit_file_features(Elf *, int, int, char *, bool);
static bool get_file_features(Elf *, int, int, uint32_t *, uint64_t *, bool);
static void print_features(void);
static bool print_file_features(Elf *, int, int, char *, bool);
static void usage(void) __dead2;

struct ControlFeatures {
	const char *alias;
	unsigned long value;
	const char *desc;
};

static struct ControlFeatures featurelist[] = {
	{ "noaslr",	NT_FREEBSD_FCTL_ASLR_DISABLE,	"Disable ASLR" },
	{ "noprotmax",	NT_FREEBSD_FCTL_PROTMAX_DISABLE,
	    "Disable implicit PROT_MAX" },
	{ "nostackgap",	NT_FREEBSD_FCTL_STKGAP_DISABLE, "Disable stack gap" },
	{ "wxneeded",	NT_FREEBSD_FCTL_WXNEEDED, "Requires W+X mappings" },
	{ "la48",	NT_FREEBSD_FCTL_LA48,
	    "amd64: Limit user virtual addresses to 48 bits" },
	{ "la57",	NT_FREEBSD_FCTL_LA57,
	    "amd64: Allow the use of 57-bit virtual addresses when available" },
};

static struct option long_opts[] = {
	{ "help",	no_argument,	NULL,	'h' },
	{ NULL,		0,		NULL,	0 }
};

#if BYTE_ORDER == LITTLE_ENDIAN
#define	HOST_ENDIAN	ELFDATA2LSB
#define	SWAP_ENDIAN	ELFDATA2MSB
#else
#define	HOST_ENDIAN	ELFDATA2MSB
#define	SWAP_ENDIAN	ELFDATA2LSB
#endif

static bool iflag;

int
main(int argc, char **argv)
{
	GElf_Ehdr ehdr;
	Elf *elf;
	Elf_Kind kind;
	int ch, fd, retval;
	char *features;
	bool editfeatures, lflag, endian_swap;

	lflag = 0;
	editfeatures = false;
	retval = 0;
	features = NULL;

	if (elf_version(EV_CURRENT) == EV_NONE)
		errx(EXIT_FAILURE, "elf_version error");

	while ((ch = getopt_long(argc, argv, "hile:", long_opts, NULL)) != -1) {
		switch (ch) {
		case 'i':
			iflag = true;
			break;
		case 'l':
			print_features();
			lflag = true;
			break;
		case 'e':
			if (features != NULL)
				errx(1, "-e may be specified only once");
			features = optarg;
			editfeatures = true;
			break;
		case 'h':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	if (argc == 0) {
		if (lflag)
			exit(0);
		else {
			warnx("no file(s) specified");
			usage();
		}
	}

	while (argc) {
		elf = NULL;

		if ((fd = open(argv[0],
		    editfeatures ? O_RDWR : O_RDONLY, 0)) < 0) {
			warn("error opening file %s", argv[0]);
			retval = 1;
			goto fail;
		}

		if ((elf = elf_begin(fd, ELF_C_READ, NULL)) == NULL) {
			warnx("elf_begin failed: %s", elf_errmsg(-1));
			retval = 1;
			goto fail;
		}

		if ((kind = elf_kind(elf)) != ELF_K_ELF) {
			if (kind == ELF_K_AR)
				warnx("file '%s' is an archive", argv[0]);
			else
				warnx("file '%s' is not an ELF file", argv[0]);
			retval = 1;
			goto fail;
		}

		if (gelf_getehdr(elf, &ehdr) == NULL) {
			warnx("gelf_getehdr: %s", elf_errmsg(-1));
			retval = 1;
			goto fail;
		}

		if (ehdr.e_ident[EI_DATA] == HOST_ENDIAN) {
			endian_swap = false;
		} else if (ehdr.e_ident[EI_DATA] == SWAP_ENDIAN) {
			endian_swap = true;
		} else {
			warnx("file endianness unknown");
			retval = 1;
			goto fail;
		}

		if (!editfeatures) {
			if (!print_file_features(elf, ehdr.e_phnum, fd,
			    argv[0], endian_swap)) {
				retval = 1;
				goto fail;
			}
		} else if (!edit_file_features(elf, ehdr.e_phnum, fd,
		    features, endian_swap)) {
			retval = 1;
			goto fail;
		}
fail:
		if (elf != NULL)
			elf_end(elf);

		if (fd >= 0)
			close(fd);

		argc--;
		argv++;
	}

	return (retval);
}

#define USAGE_MESSAGE \
	"\
Usage: %s [options] file...\n\
  Set or display the control features for an ELF object.\n\n\
  Supported options are:\n\
  -l                        List known control features.\n\
  -i                        Ignore unknown features.\n\
  -e [+-=]feature,list      Edit features from a comma separated list.\n\
  -h | --help               Print a usage message and exit.\n"

static void
usage(void)
{

	fprintf(stderr, USAGE_MESSAGE, ELFTC_GETPROGNAME());
	exit(1);
}

static bool
convert_to_feature_val(const char *feature_str, uint32_t *feature_val)
{
	char *feature, *feature_tmp;
	int i, len;
	uint32_t input;
	char operation;

	input = 0;
	operation = *feature_str;
	feature_str++;

	if (operation != '+' && operation != '-' && operation != '=')
		errx(1, "'%c' not an operator - use '+', '-', '='", operation);

	if ((feature_tmp = strdup(feature_str)) == NULL)
		err(1, "strdup");
	len = nitems(featurelist);
	while ((feature = strsep(&feature_tmp, ",")) != NULL) {
		for (i = 0; i < len; ++i) {
			if (strcmp(featurelist[i].alias, feature) == 0) {
				input |= featurelist[i].value;
				break;
			}
			/* XXX Backwards compatibility for "no"-prefix flags. */
			if (strncmp(featurelist[i].alias, "no", 2) == 0 &&
			    strcmp(featurelist[i].alias + 2, feature) == 0) {
				input |= featurelist[i].value;
				warnx(
				    "interpreting %s as %s; please specify %s",
				    feature, featurelist[i].alias,
				    featurelist[i].alias);
				break;
			}
		}
		if (i == len) {
			if (isdigit(feature[0])) {
				char *eptr;
				unsigned long long val;

				errno = 0;
				val = strtoll(feature, &eptr, 0);
				if (eptr == feature || *eptr != '\0')
					errno = EINVAL;
				else if (val > UINT32_MAX)
					errno = ERANGE;
				if (errno != 0) {
					warn("%s invalid", feature);
					free(feature_tmp);
					return (false);
				}
				input |= val;
			} else {
				warnx("%s is not a valid feature", feature);
				if (!iflag) {
					free(feature_tmp);
					return (false);
				}
			}
		}
	}

	if (operation == '+') {
		*feature_val |= input;
	} else if (operation == '=') {
		*feature_val = input;
	} else if (operation == '-') {
		*feature_val &= ~input;
	}
	free(feature_tmp);
	return (true);
}

static bool
edit_file_features(Elf *elf, int phcount, int fd, char *val, bool endian_swap)
{
	uint32_t features, prev_features;
	uint64_t off;

	if (!get_file_features(elf, phcount, fd, &features, &off,
	    endian_swap)) {
		warnx("NT_FREEBSD_FEATURE_CTL note not found");
		return (false);
	}

	prev_features = features;
	if (!convert_to_feature_val(val, &features))
		return (false);
	/* Avoid touching file if no change. */
	if (features == prev_features)
		return (true);

	if (endian_swap)
		features = bswap32(features);

	if (lseek(fd, off, SEEK_SET) == -1 ||
	    write(fd, &features, sizeof(features)) <
	    (ssize_t)sizeof(features)) {
		warnx("error writing feature value");
		return (false);
	}
	return (true);
}

static void
print_features(void)
{
	size_t i;

	printf("Known features are:\n");
	for (i = 0; i < nitems(featurelist); ++i)
		printf("%-16s%s\n", featurelist[i].alias,
		    featurelist[i].desc);
}

static bool
print_file_features(Elf *elf, int phcount, int fd, char *filename,
    bool endian_swap)
{
	uint32_t features;
	unsigned long i;

	if (!get_file_features(elf, phcount, fd, &features, NULL,
	    endian_swap)) {
		return (false);
	}

	printf("File '%s' features:\n", filename);
	for (i = 0; i < nitems(featurelist); ++i) {
		printf("%-16s'%s' is ", featurelist[i].alias,
		    featurelist[i].desc);

		if ((featurelist[i].value & features) == 0)
			printf("un");

		printf("set.\n");
	}
	return (true);
}

static bool
get_file_features(Elf *elf, int phcount, int fd, uint32_t *features,
    uint64_t *off, bool endian_swap)
{
	GElf_Phdr phdr;
	Elf_Note note;
	unsigned long read_total;
	int namesz, descsz, i;
	char *name;

	/*
	 * Go through each program header to find one that is of type PT_NOTE
	 * and has a note for feature control.
	 */
	for (i = 0; i < phcount; ++i) {
		if (gelf_getphdr(elf, i, &phdr) == NULL) {
			warnx("gelf_getphdr failed: %s", elf_errmsg(-1));
			return (false);
		}

		if (phdr.p_type != PT_NOTE)
			continue;

		if (lseek(fd, phdr.p_offset, SEEK_SET) < 0) {
			warn("lseek() failed:");
			return (false);
		}

		read_total = 0;
		while (read_total < phdr.p_filesz) {
			if (read(fd, &note, sizeof(note)) <
			    (ssize_t)sizeof(note)) {
				warnx("elf note header too short");
				return (false);
			}
			read_total += sizeof(note);

			if (endian_swap) {
				note.n_namesz = bswap32(note.n_namesz);
				note.n_descsz = bswap32(note.n_descsz);
				note.n_type = bswap32(note.n_type);
			}

			/*
			 * XXX: Name and descriptor are 4 byte aligned, however,
			 * the size given doesn't include the padding.
			 */
			namesz = roundup2(note.n_namesz, 4);
			name = malloc(namesz);
			if (name == NULL) {
				warn("malloc() failed.");
				return (false);
			}
			descsz = roundup2(note.n_descsz, 4);
			if (read(fd, name, namesz) < namesz) {
				warnx("elf note name too short");
				free(name);
				return (false);
			}
			read_total += namesz;

			if (note.n_namesz != 8 ||
			    strncmp("FreeBSD", name, 7) != 0 ||
			    note.n_type != NT_FREEBSD_FEATURE_CTL) {
				/* Not the right note. Skip the description */
				if (lseek(fd, descsz, SEEK_CUR) < 0) {
					warn("lseek() failed.");
					free(name);
					return (false);
				}
				read_total += descsz;
				free(name);
				continue;
			}

			if (note.n_descsz < sizeof(uint32_t)) {
				warnx("Feature descriptor can't "
				    "be less than 4 bytes");
				free(name);
				return (false);
			}

			/*
			 * XXX: For now we look at only 4 bytes of the
			 * descriptor. This should respect descsz.
			 */
			if (note.n_descsz > sizeof(uint32_t))
				warnx("Feature note is bigger than expected");
			if (read(fd, features, sizeof(uint32_t)) <
			    (ssize_t)sizeof(uint32_t)) {
				warnx("feature note data too short");
				free(name);
				return (false);
			}
			if (endian_swap)
				*features = bswap32(*features);
			if (off != NULL)
				*off = phdr.p_offset + read_total;
			free(name);
			return (true);
		}
	}

	warnx("NT_FREEBSD_FEATURE_CTL note not found");
	return (false);
}
