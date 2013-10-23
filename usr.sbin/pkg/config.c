/*-
 * Copyright (c) 2013 Baptiste Daroussin <bapt@FreeBSD.org>
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/sbuf.h>
#include <sys/elf_common.h>
#include <sys/endian.h>

#include <bsdyml.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <gelf.h>
#include <inttypes.h>
#include <paths.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include "elf_tables.h"
#include "config.h"

#define roundup2(x, y)	(((x)+((y)-1))&(~((y)-1))) /* if y is powers of two */

struct config_entry {
	uint8_t type;
	const char *key;
	const char *val;
	char *value;
	bool envset;
};

static struct config_entry c[] = {
	[PACKAGESITE] = {
		PKG_CONFIG_STRING,
		"PACKAGESITE",
		"http://pkg.FreeBSD.org/${ABI}/latest",
		NULL,
		false,
	},
	[ABI] = {
		PKG_CONFIG_STRING,
		"ABI",
		NULL,
		NULL,
		false,
	},
	[MIRROR_TYPE] = {
		PKG_CONFIG_STRING,
		"MIRROR_TYPE",
		"SRV",
		NULL,
		false,
	},
	[ASSUME_ALWAYS_YES] = {
		PKG_CONFIG_BOOL,
		"ASSUME_ALWAYS_YES",
		"NO",
		NULL,
		false,
	}
};

static const char *
elf_corres_to_string(struct _elf_corres *m, int e)
{
	int i;

	for (i = 0; m[i].string != NULL; i++)
		if (m[i].elf_nb == e)
			return (m[i].string);

	return ("unknown");
}

static int
pkg_get_myabi(char *dest, size_t sz)
{
	Elf *elf;
	Elf_Data *data;
	Elf_Note note;
	Elf_Scn *scn;
	char *src, *osname;
	const char *abi, *fpu;
	GElf_Ehdr elfhdr;
	GElf_Shdr shdr;
	int fd, i, ret;
	uint32_t version;

	version = 0;
	ret = -1;
	scn = NULL;
	abi = NULL;

	if (elf_version(EV_CURRENT) == EV_NONE) {
		warnx("ELF library initialization failed: %s",
		    elf_errmsg(-1));
		return (-1);
	}

	if ((fd = open(_PATH_BSHELL, O_RDONLY)) < 0) {
		warn("open()");
		return (-1);
	}

	if ((elf = elf_begin(fd, ELF_C_READ, NULL)) == NULL) {
		ret = -1;
		warnx("elf_begin() failed: %s.", elf_errmsg(-1));
		goto cleanup;
	}

	if (gelf_getehdr(elf, &elfhdr) == NULL) {
		ret = -1;
		warn("getehdr() failed: %s.", elf_errmsg(-1));
		goto cleanup;
	}
	while ((scn = elf_nextscn(elf, scn)) != NULL) {
		if (gelf_getshdr(scn, &shdr) != &shdr) {
			ret = -1;
			warn("getshdr() failed: %s.", elf_errmsg(-1));
			goto cleanup;
		}

		if (shdr.sh_type == SHT_NOTE)
			break;
	}

	if (scn == NULL) {
		ret = -1;
		warn("failed to get the note section");
		goto cleanup;
	}

	data = elf_getdata(scn, NULL);
	src = data->d_buf;
	for (;;) {
		memcpy(&note, src, sizeof(Elf_Note));
		src += sizeof(Elf_Note);
		if (note.n_type == NT_VERSION)
			break;
		src += note.n_namesz + note.n_descsz;
	}
	osname = src;
	src += roundup2(note.n_namesz, 4);
	if (elfhdr.e_ident[EI_DATA] == ELFDATA2MSB)
		version = be32dec(src);
	else
		version = le32dec(src);

	for (i = 0; osname[i] != '\0'; i++)
		osname[i] = (char)tolower(osname[i]);

	snprintf(dest, sz, "%s:%d:%s:%s",
	    osname, version / 100000,
	    elf_corres_to_string(mach_corres, (int)elfhdr.e_machine),
	    elf_corres_to_string(wordsize_corres,
	    (int)elfhdr.e_ident[EI_CLASS]));

	ret = 0;

	switch (elfhdr.e_machine) {
	case EM_ARM:
		/* FreeBSD doesn't support the hard-float ABI yet */
		fpu = "softfp";
		if ((elfhdr.e_flags & 0xFF000000) != 0) {
			/* This is an EABI file, the conformance level is set */
			abi = "eabi";
		} else if (elfhdr.e_ident[EI_OSABI] != ELFOSABI_NONE) {
			/*
			 * EABI executables all have this field set to
			 * ELFOSABI_NONE, therefore it must be an oabi file.
			 */
			abi = "oabi";
		} else {
			ret = 1;
			goto cleanup;
		}
		snprintf(dest + strlen(dest), sz - strlen(dest),
		    ":%s:%s:%s", elf_corres_to_string(endian_corres,
		    (int)elfhdr.e_ident[EI_DATA]),
		    abi, fpu);
		break;
	case EM_MIPS:
		/*
		 * this is taken from binutils sources:
		 * include/elf/mips.h
		 * mapping is figured out from binutils:
		 * gas/config/tc-mips.c
		 */
		switch (elfhdr.e_flags & EF_MIPS_ABI) {
		case E_MIPS_ABI_O32:
			abi = "o32";
			break;
		case E_MIPS_ABI_N32:
			abi = "n32";
			break;
		default:
			if (elfhdr.e_ident[EI_DATA] ==
			    ELFCLASS32)
				abi = "o32";
			else if (elfhdr.e_ident[EI_DATA] ==
			    ELFCLASS64)
				abi = "n64";
			break;
		}
		snprintf(dest + strlen(dest), sz - strlen(dest),
		    ":%s:%s", elf_corres_to_string(endian_corres,
		    (int)elfhdr.e_ident[EI_DATA]), abi);
		break;
	}

cleanup:
	if (elf != NULL)
		elf_end(elf);

	close(fd);
	return (ret);
}

static void
subst_packagesite(const char *abi)
{
	struct sbuf *newval;
	const char *variable_string;
	const char *oldval;

	if (c[PACKAGESITE].value != NULL)
		oldval = c[PACKAGESITE].value;
	else
		oldval = c[PACKAGESITE].val;

	if ((variable_string = strstr(oldval, "${ABI}")) == NULL)
		return;

	newval = sbuf_new_auto();
	sbuf_bcat(newval, oldval, variable_string - oldval);
	sbuf_cat(newval, abi);
	sbuf_cat(newval, variable_string + strlen("${ABI}"));
	sbuf_finish(newval);

	free(c[PACKAGESITE].value);
	c[PACKAGESITE].value = strdup(sbuf_data(newval));
}

static void
config_parse(yaml_document_t *doc, yaml_node_t *node)
{
	yaml_node_pair_t *pair;
	yaml_node_t *key, *val;
	struct sbuf *buf = sbuf_new_auto();
	int i;
	size_t j;

	pair = node->data.mapping.pairs.start;

	while (pair < node->data.mapping.pairs.top) {
		key = yaml_document_get_node(doc, pair->key);
		val = yaml_document_get_node(doc, pair->value);

		/*
		 * ignoring silently empty keys can be empty lines
		 * or user mistakes
		 */
		if (key->data.scalar.length <= 0) {
			++pair;
			continue;
		}

		/*
		 * silently skip on purpose to allow user to leave
		 * empty lines without complaining
		 */
		if (val->type == YAML_NO_NODE ||
		    (val->type == YAML_SCALAR_NODE &&
		     val->data.scalar.length <= 0)) {
			++pair;
			continue;
		}

		sbuf_clear(buf);
		for (j = 0; j < strlen(key->data.scalar.value); ++j)
			sbuf_putc(buf, toupper(key->data.scalar.value[j]));

		sbuf_finish(buf);
		for (i = 0; i < CONFIG_SIZE; i++) {
			if (strcmp(sbuf_data(buf), c[i].key) == 0)
				break;
		}

		if (i == CONFIG_SIZE) {
			++pair;
			continue;
		}

		/* env has priority over config file */
		if (c[i].envset) {
			++pair;
			continue;
		}

		c[i].value = strdup(val->data.scalar.value);
		++pair;
	}

	sbuf_delete(buf);
}

int
config_init(void)
{
	FILE *fp;
	yaml_parser_t parser;
	yaml_document_t doc;
	yaml_node_t *node;
	const char *val;
	int i;
	const char *localbase;
	char confpath[MAXPATHLEN];
	char abi[BUFSIZ];

	for (i = 0; i < CONFIG_SIZE; i++) {
		val = getenv(c[i].key);
		if (val != NULL) {
			c[i].val = val;
			c[i].envset = true;
		}
	}

	localbase = getenv("LOCALBASE") ? getenv("LOCALBASE") : _LOCALBASE;
	snprintf(confpath, sizeof(confpath), "%s/etc/pkg.conf", localbase);

	if ((fp = fopen(confpath, "r")) == NULL) {
		if (errno != ENOENT)
			err(EXIT_FAILURE, "Unable to open configuration file %s", confpath);
		/* no configuration present */
		goto finalize;
	}

	yaml_parser_initialize(&parser);
	yaml_parser_set_input_file(&parser, fp);
	yaml_parser_load(&parser, &doc);

	node = yaml_document_get_root_node(&doc);

	if (node != NULL) {
		if (node->type != YAML_MAPPING_NODE)
			warnx("Invalid configuration format, ignoring the configuration file");
		else
			config_parse(&doc, node);
	} else {
		warnx("Invalid configuration format, ignoring the configuration file");
	}

	yaml_document_delete(&doc);
	yaml_parser_delete(&parser);

finalize:
	if (c[ABI].val == NULL && c[ABI].value == NULL) {
		if (pkg_get_myabi(abi, BUFSIZ) != 0)
			errx(EXIT_FAILURE, "Failed to determine the system ABI");
		c[ABI].val = abi;
	}

	subst_packagesite(c[ABI].value != NULL ? c[ABI].value : c[ABI].val);

	return (0);
}

int
config_string(pkg_config_key k, const char **val)
{
	if (c[k].type != PKG_CONFIG_STRING)
		return (-1);

	if (c[k].value != NULL)
		*val = c[k].value;
	else
		*val = c[k].val;

	return (0);
}

int
config_bool(pkg_config_key k, bool *val)
{
	const char *value;

	if (c[k].type != PKG_CONFIG_BOOL)
		return (-1);

	*val = false;

	if (c[k].value != NULL)
		value = c[k].value;
	else
		value = c[k].val;

	if (strcasecmp(value, "true") == 0 ||
	    strcasecmp(value, "yes") == 0 ||
	    strcasecmp(value, "on") == 0 ||
	    *value == '1')
		*val = true;

	return (0);
}

void
config_finish(void) {
	int i;

	for (i = 0; i < CONFIG_SIZE; i++)
		free(c[i].value);
}
