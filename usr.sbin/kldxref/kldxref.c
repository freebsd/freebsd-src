/*-
 * SPDX-License-Identifier: BSD-4-Clause
 *
 * Copyright (c) 2000, Boris Popov
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
#include <sys/endian.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <sys/module.h>

#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fts.h>
#include <gelf.h>
#include <libelf.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ef.h"

#define	MAXRECSIZE	(64 << 10)	/* 64k */
#define check(val)	if ((error = (val)) != 0) break

static bool dflag;	/* do not create a hint file, only write on stdout */
static int verbose;

static FILE *fxref;	/* current hints file */
static int byte_order;
static GElf_Ehdr ehdr;
static char *ehdr_filename;

static const char *xref_file = "linker.hints";

/*
 * A record is stored in the static buffer recbuf before going to disk.
 */
static char recbuf[MAXRECSIZE];
static int recpos;	/* current write position */
static int reccnt;	/* total record written to this file so far */

static void
intalign(void)
{

	recpos = roundup2(recpos, sizeof(int));
}

static void
write_int(int val)
{
	char buf[4];

	assert(byte_order != ELFDATANONE);
	if (byte_order == ELFDATA2LSB)
		le32enc(buf, val);
	else
		be32enc(buf, val);
	fwrite(buf, sizeof(buf), 1, fxref);
}

static void
record_start(void)
{

	recpos = 0;
	memset(recbuf, 0, MAXRECSIZE);
}

static int
record_end(void)
{

	if (recpos == 0) {
		/*
		 * Pretend to have written a record in debug mode so
		 * the architecture check works.
		 */
		if (dflag)
			reccnt++;
		return (0);
	}

	if (reccnt == 0) {
		/* File version record. */
		write_int(1);
	}

	reccnt++;
	intalign();
	write_int(recpos);
	return (fwrite(recbuf, recpos, 1, fxref) != 1 ? errno : 0);
}

static int
record_buf(const void *buf, size_t size)
{

	if (MAXRECSIZE - recpos < size)
		errx(1, "record buffer overflow");
	memcpy(recbuf + recpos, buf, size);
	recpos += size;
	return (0);
}

/*
 * An int is stored in target byte order and aligned
 */
static int
record_int(int val)
{
	char buf[4];

	assert(byte_order != ELFDATANONE);
	if (byte_order == ELFDATA2LSB)
		le32enc(buf, val);
	else
		be32enc(buf, val);

	intalign();
	return (record_buf(buf, sizeof(buf)));
}

/*
 * A string is stored as 1-byte length plus data, no padding
 */
static int
record_string(const char *str)
{
	int error;
	size_t len;
	u_char val;
	
	if (dflag)
		return (0);
	val = len = strlen(str);
	if (len > 255)
		errx(1, "string %s too long", str);
	error = record_buf(&val, sizeof(val));
	if (error != 0)
		return (error);
	return (record_buf(str, len));
}

/* From sys/isa/pnp.c */
static char *
pnp_eisaformat(uint32_t id)
{
	uint8_t *data;
	static char idbuf[8];
	const char  hextoascii[] = "0123456789abcdef";

	id = htole32(id);
	data = (uint8_t *)&id;
	idbuf[0] = '@' + ((data[0] & 0x7c) >> 2);
	idbuf[1] = '@' + (((data[0] & 0x3) << 3) + ((data[1] & 0xe0) >> 5));
	idbuf[2] = '@' + (data[1] & 0x1f);
	idbuf[3] = hextoascii[(data[2] >> 4)];
	idbuf[4] = hextoascii[(data[2] & 0xf)];
	idbuf[5] = hextoascii[(data[3] >> 4)];
	idbuf[6] = hextoascii[(data[3] & 0xf)];
	idbuf[7] = 0;
	return (idbuf);
}

struct pnp_elt
{
	int	pe_kind;	/* What kind of entry */
#define TYPE_SZ_MASK	0x0f
#define TYPE_FLAGGED	0x10	/* all f's is a wildcard */
#define	TYPE_INT	0x20	/* Is a number */
#define TYPE_PAIRED	0x40
#define TYPE_LE		0x80	/* Matches <= this value */
#define TYPE_GE		0x100	/* Matches >= this value */
#define TYPE_MASK	0x200	/* Specifies a mask to follow */
#define TYPE_U8		(1 | TYPE_INT)
#define TYPE_V8		(1 | TYPE_INT | TYPE_FLAGGED)
#define TYPE_G16	(2 | TYPE_INT | TYPE_GE)
#define TYPE_L16	(2 | TYPE_INT | TYPE_LE)
#define TYPE_M16	(2 | TYPE_INT | TYPE_MASK)
#define TYPE_U16	(2 | TYPE_INT)
#define TYPE_V16	(2 | TYPE_INT | TYPE_FLAGGED)
#define TYPE_U32	(4 | TYPE_INT)
#define TYPE_V32	(4 | TYPE_INT | TYPE_FLAGGED)
#define TYPE_W32	(4 | TYPE_INT | TYPE_PAIRED)
#define TYPE_D		7
#define TYPE_Z		8
#define TYPE_P		9
#define TYPE_E		10
#define TYPE_T		11
	int	pe_offset;	/* Offset within the element */
	char *	pe_key;		/* pnp key name */
	TAILQ_ENTRY(pnp_elt) next; /* Link */
};
typedef TAILQ_HEAD(pnp_head, pnp_elt) pnp_list;

/*
 * this function finds the data from the pnp table, as described by the
 * description and creates a new output (new_desc). This output table
 * is a form that's easier for the agent that's automatically loading the
 * modules.
 *
 * The format output is the simplified string from this routine in the
 * same basic format as the pnp string, as documented in sys/module.h.
 * First a string describing the format is output, the a count of the
 * number of records, then each record. The format string also describes
 * the length of each entry (though it isn't a fixed length when strings
 * are present).
 *
 *	type	Output		Meaning
 *	I	uint32_t	Integer equality comparison
 *	J	uint32_t	Pair of uint16_t fields converted to native
 *				byte order. The two fields both must match.
 *	G	uint32_t	Greater than or equal to
 *	L	uint32_t	Less than or equal to
 *	M	uint32_t	Mask of which fields to test. Fields that
 *				take up space increment the count. This
 *				field must be first, and resets the count.
 *	D	string		Description of the device this pnp info is for
 *	Z	string		pnp string must match this
 *	T	nothing		T fields set pnp values that must be true for
 *				the entire table.
 * Values are packed the same way that other values are packed in this file.
 * Strings and int32_t's start on a 32-bit boundary and are padded with 0
 * bytes. Objects that are smaller than uint32_t are converted, without
 * sign extension to uint32_t to simplify parsing downstream.
 */
static int
parse_pnp_list(struct elf_file *ef, const char *desc, char **new_desc,
    pnp_list *list)
{
	const char *walker, *ep;
	const char *colon, *semi;
	struct pnp_elt *elt;
	char type[8], key[32];
	int off;
	size_t new_desc_size;
	FILE *fp;

	TAILQ_INIT(list);
	walker = desc;
	ep = desc + strlen(desc);
	off = 0;
	fp = open_memstream(new_desc, &new_desc_size);
	if (fp == NULL)
		err(1, "Could not open new memory stream");
	if (verbose > 1)
		printf("Converting %s into a list\n", desc);
	while (walker < ep) {
		colon = strchr(walker, ':');
		semi = strchr(walker, ';');
		if (semi != NULL && semi < colon)
			goto err;
		if (colon - walker > sizeof(type))
			goto err;
		strncpy(type, walker, colon - walker);
		type[colon - walker] = '\0';
		if (semi != NULL) {
			if (semi - colon >= sizeof(key))
				goto err;
			strncpy(key, colon + 1, semi - colon - 1);
			key[semi - colon - 1] = '\0';
			walker = semi + 1;
			/* Fail safe if we have spaces after ; */
			while (walker < ep && isspace(*walker))
				walker++;
		} else {
			if (strlen(colon + 1) >= sizeof(key))
				goto err;
			strcpy(key, colon + 1);
			walker = ep;
		}
		if (verbose > 1)
			printf("Found type %s for name %s\n", type, key);
		/* Skip pointer place holders */
		if (strcmp(type, "P") == 0) {
			off += elf_pointer_size(ef);
			continue;
		}

		/*
		 * Add a node of the appropriate type
		 */
		elt = malloc(sizeof(struct pnp_elt) + strlen(key) + 1);
		TAILQ_INSERT_TAIL(list, elt, next);
		elt->pe_key = (char *)(elt + 1);
		elt->pe_offset = off;
		if (strcmp(type, "U8") == 0)
			elt->pe_kind = TYPE_U8;
		else if (strcmp(type, "V8") == 0)
			elt->pe_kind = TYPE_V8;
		else if (strcmp(type, "G16") == 0)
			elt->pe_kind = TYPE_G16;
		else if (strcmp(type, "L16") == 0)
			elt->pe_kind = TYPE_L16;
		else if (strcmp(type, "M16") == 0)
			elt->pe_kind = TYPE_M16;
		else if (strcmp(type, "U16") == 0)
			elt->pe_kind = TYPE_U16;
		else if (strcmp(type, "V16") == 0)
			elt->pe_kind = TYPE_V16;
		else if (strcmp(type, "U32") == 0)
			elt->pe_kind = TYPE_U32;
		else if (strcmp(type, "V32") == 0)
			elt->pe_kind = TYPE_V32;
		else if (strcmp(type, "W32") == 0)
			elt->pe_kind = TYPE_W32;
		else if (strcmp(type, "D") == 0)	/* description char * */
			elt->pe_kind = TYPE_D;
		else if (strcmp(type, "Z") == 0)	/* char * to match */
			elt->pe_kind = TYPE_Z;
		else if (strcmp(type, "P") == 0)	/* Pointer -- ignored */
			elt->pe_kind = TYPE_P;
		else if (strcmp(type, "E") == 0)	/* EISA PNP ID, as uint32_t */
			elt->pe_kind = TYPE_E;
		else if (strcmp(type, "T") == 0)
			elt->pe_kind = TYPE_T;
		else
			goto err;
		/*
		 * Maybe the rounding here needs to be more nuanced and/or somehow
		 * architecture specific. Fortunately, most tables in the system
		 * have sane ordering of types.
		 */
		if (elt->pe_kind & TYPE_INT) {
			elt->pe_offset = roundup2(elt->pe_offset, elt->pe_kind & TYPE_SZ_MASK);
			off = elt->pe_offset + (elt->pe_kind & TYPE_SZ_MASK);
		} else if (elt->pe_kind == TYPE_E) {
			/* Type E stored as Int, displays as string */
			elt->pe_offset = roundup2(elt->pe_offset, sizeof(uint32_t));
			off = elt->pe_offset + sizeof(uint32_t);
		} else if (elt->pe_kind == TYPE_T) {
			/* doesn't actually consume space in the table */
			off = elt->pe_offset;
		} else {
			elt->pe_offset = roundup2(elt->pe_offset, elf_pointer_size(ef));
			off = elt->pe_offset + elf_pointer_size(ef);
		}
		if (elt->pe_kind & TYPE_PAIRED) {
			char *word, *ctx, newtype;

			for (word = strtok_r(key, "/", &ctx);
			     word; word = strtok_r(NULL, "/", &ctx)) {
				newtype = elt->pe_kind & TYPE_FLAGGED ? 'J' : 'I';
				fprintf(fp, "%c:%s;", newtype, word);
			}
		}
		else {
			char newtype;

			if (elt->pe_kind & TYPE_FLAGGED)
				newtype = 'J';
			else if (elt->pe_kind & TYPE_GE)
				newtype = 'G';
			else if (elt->pe_kind & TYPE_LE)
				newtype = 'L';
			else if (elt->pe_kind & TYPE_MASK)
				newtype = 'M';
			else if (elt->pe_kind & TYPE_INT)
				newtype = 'I';
			else if (elt->pe_kind == TYPE_D)
				newtype = 'D';
			else if (elt->pe_kind == TYPE_Z || elt->pe_kind == TYPE_E)
				newtype = 'Z';
			else if (elt->pe_kind == TYPE_T)
				newtype = 'T';
			else
				errx(1, "Impossible type %x\n", elt->pe_kind);
			fprintf(fp, "%c:%s;", newtype, key);
		}
	}
	if (ferror(fp) != 0) {
		fclose(fp);
		errx(1, "Exhausted space converting description %s", desc);
	}
	if (fclose(fp) != 0)
		errx(1, "Failed to close memory stream");
	return (0);
err:
	errx(1, "Parse error of description string %s", desc);
}

static void
free_pnp_list(char *new_desc, pnp_list *list)
{
	struct pnp_elt *elt, *elt_tmp;

	TAILQ_FOREACH_SAFE(elt, list, next, elt_tmp) {
		TAILQ_REMOVE(list, elt, next);
		free(elt);
	}
	free(new_desc);
}

static uint16_t
parse_16(const void *p)
{
	if (byte_order == ELFDATA2LSB)
		return (le16dec(p));
	else
		return (be16dec(p));
}

static uint32_t
parse_32(const void *p)
{
	if (byte_order == ELFDATA2LSB)
		return (le32dec(p));
	else
		return (be32dec(p));
}

static void
parse_pnp_entry(struct elf_file *ef, struct pnp_elt *elt, const char *walker)
{
	uint8_t v1;
	uint16_t v2;
	uint32_t v4;
	int	value;
	char buffer[1024];

	if (elt->pe_kind == TYPE_W32) {
		v4 = parse_32(walker + elt->pe_offset);
		value = v4 & 0xffff;
		record_int(value);
		if (verbose > 1)
			printf("W32:%#x", value);
		value = (v4 >> 16) & 0xffff;
		record_int(value);
		if (verbose > 1)
			printf(":%#x;", value);
	} else if (elt->pe_kind & TYPE_INT) {
		switch (elt->pe_kind & TYPE_SZ_MASK) {
		case 1:
			memcpy(&v1, walker + elt->pe_offset, sizeof(v1));
			if ((elt->pe_kind & TYPE_FLAGGED) && v1 == 0xff)
				value = -1;
			else
				value = v1;
			break;
		case 2:
			v2 = parse_16(walker + elt->pe_offset);
			if ((elt->pe_kind & TYPE_FLAGGED) && v2 == 0xffff)
				value = -1;
			else
				value = v2;
			break;
		case 4:
			v4 = parse_32(walker + elt->pe_offset);
			if ((elt->pe_kind & TYPE_FLAGGED) && v4 == 0xffffffff)
				value = -1;
			else
				value = v4;
			break;
		default:
			errx(1, "Invalid size somehow %#x", elt->pe_kind);
		}
		if (verbose > 1)
			printf("I:%#x;", value);
		record_int(value);
	} else if (elt->pe_kind == TYPE_T) {
		/* Do nothing */
	} else { /* E, Z or D -- P already filtered */
		if (elt->pe_kind == TYPE_E) {
			v4 = parse_32(walker + elt->pe_offset);
			strcpy(buffer, pnp_eisaformat(v4));
		} else {
			GElf_Addr address;

			address = elf_address_from_pointer(ef, walker +
			    elt->pe_offset);
			buffer[0] = '\0';
			if (address != 0) {
				elf_read_string(ef, address, buffer,
				    sizeof(buffer));
				buffer[sizeof(buffer) - 1] = '\0';
			}
		}
		if (verbose > 1)
			printf("%c:%s;", elt->pe_kind == TYPE_E ? 'E' :
			    (elt->pe_kind == TYPE_Z ? 'Z' : 'D'), buffer);
		record_string(buffer);
	}
}

static void
record_pnp_info(struct elf_file *ef, const char *cval,
    struct Gmod_pnp_match_info *pnp, const char *descr)
{
	pnp_list list;
	struct pnp_elt *elt;
	char *new_descr, *walker;
	void *table;
	size_t len;
	int error, i;

	if (verbose > 1)
		printf("  pnp info for bus %s format %s %d entries of %d bytes\n",
		    cval, descr, pnp->num_entry, pnp->entry_len);

	/*
	 * Parse descr to weed out the chaff and to create a list
	 * of offsets to output.
	 */
	parse_pnp_list(ef, descr, &new_descr, &list);
	record_int(MDT_PNP_INFO);
	record_string(cval);
	record_string(new_descr);
	record_int(pnp->num_entry);
	len = pnp->num_entry * pnp->entry_len;
	error = elf_read_relocated_data(ef, pnp->table, len, &table);
	if (error != 0) {
		free_pnp_list(new_descr, &list);
		return;
	}

	/*
	 * Walk the list and output things. We've collapsed all the
	 * variant forms of the table down to just ints and strings.
	 */
	walker = table;
	for (i = 0; i < pnp->num_entry; i++) {
		TAILQ_FOREACH(elt, &list, next) {
			parse_pnp_entry(ef, elt, walker);
		}
		if (verbose > 1)
			printf("\n");
		walker += pnp->entry_len;
	}

	/* Now free it */
	free_pnp_list(new_descr, &list);
	free(table);
}

static int
parse_entry(struct Gmod_metadata *md, const char *cval,
    struct elf_file *ef, const char *kldname)
{
	struct Gmod_depend mdp;
	struct Gmod_version mdv;
	struct Gmod_pnp_match_info pnp;
	char descr[1024];
	GElf_Addr data;
	int error;

	data = md->md_data;
	error = 0;
	record_start();
	switch (md->md_type) {
	case MDT_DEPEND:
		if (!dflag)
			break;
		check(elf_read_mod_depend(ef, data, &mdp));
		printf("  depends on %s.%d (%d,%d)\n", cval,
		    mdp.md_ver_preferred, mdp.md_ver_minimum, mdp.md_ver_maximum);
		break;
	case MDT_VERSION:
		check(elf_read_mod_version(ef, data, &mdv));
		if (dflag) {
			printf("  interface %s.%d\n", cval, mdv.mv_version);
		} else {
			record_int(MDT_VERSION);
			record_string(cval);
			record_int(mdv.mv_version);
			record_string(kldname);
		}
		break;
	case MDT_MODULE:
		if (dflag) {
			printf("  module %s\n", cval);
		} else {
			record_int(MDT_MODULE);
			record_string(cval);
			record_string(kldname);
		}
		break;
	case MDT_PNP_INFO:
		check(elf_read_mod_pnp_match_info(ef, data, &pnp));
		check(elf_read_string(ef, pnp.descr, descr, sizeof(descr)));
		if (dflag) {
			printf("  pnp info for bus %s format %s %d entries of %d bytes\n",
			    cval, descr, pnp.num_entry, pnp.entry_len);
		} else {
			record_pnp_info(ef, cval, &pnp, descr);
		}
		break;
	default:
		warnx("unknown metadata record %d in file %s", md->md_type, kldname);
	}
	if (!error)
		record_end();
	return (error);
}

static int
read_kld(char *filename, char *kldname)
{
	struct Gmod_metadata md;
	struct elf_file ef;
	GElf_Addr *p;
	int error;
	long entries, i;
	char cval[MAXMODNAME + 1];

	if (verbose || dflag)
		printf("%s\n", filename);

	error = elf_open_file(&ef, filename, verbose);
	if (error != 0)
		return (error);

	if (reccnt == 0) {
		ehdr = ef.ef_hdr;
		byte_order = elf_encoding(&ef);
		free(ehdr_filename);
		ehdr_filename = strdup(filename);
	} else if (!elf_compatible(&ef, &ehdr)) {
		warnx("%s does not match architecture of %s",
		    filename, ehdr_filename);
		elf_close_file(&ef);
		return (EINVAL);
	}

	do {
		check(elf_read_linker_set(&ef, MDT_SETNAME, &p, &entries));

		/*
		 * Do a first pass to find MDT_MODULE.  It is required to be
		 * ordered first in the output linker.hints stream because it
		 * serves as an implicit record boundary between distinct klds
		 * in the stream.  Other MDTs only make sense in the context of
		 * a specific MDT_MODULE.
		 *
		 * Some compilers (e.g., GCC 6.4.0 xtoolchain) or binutils
		 * (e.g., GNU binutils 2.32 objcopy/ld.bfd) can reorder
		 * MODULE_METADATA set entries relative to the source ordering.
		 * This is permitted by the C standard; memory layout of
		 * file-scope objects is left implementation-defined.  There is
		 * no requirement that source code ordering is retained.
		 *
		 * Handle that here by taking two passes to ensure MDT_MODULE
		 * records are emitted to linker.hints before other MDT records
		 * in the same kld.
		 */
		for (i = 0; i < entries; i++) {
			check(elf_read_mod_metadata(&ef, p[i], &md));
			check(elf_read_string(&ef, md.md_cval, cval,
			    sizeof(cval)));
			if (md.md_type == MDT_MODULE) {
				parse_entry(&md, cval, &ef, kldname);
				break;
			}
		}
		if (error != 0) {
			free(p);
			warnc(error, "error while reading %s", filename);
			break;
		}

		/*
		 * Second pass for all !MDT_MODULE entries.
		 */
		for (i = 0; i < entries; i++) {
			check(elf_read_mod_metadata(&ef, p[i], &md));
			check(elf_read_string(&ef, md.md_cval, cval,
			    sizeof(cval)));
			if (md.md_type != MDT_MODULE)
				parse_entry(&md, cval, &ef, kldname);
		}
		if (error != 0)
			warnc(error, "error while reading %s", filename);
		free(p);
	} while(0);
	elf_close_file(&ef);
	return (error);
}

/*
 * Create a temp file in directory root, make sure we don't
 * overflow the buffer for the destination name
 */
static FILE *
maketempfile(char *dest, const char *root)
{
	int fd;

	if (snprintf(dest, MAXPATHLEN, "%s/lhint.XXXXXX", root) >=
	    MAXPATHLEN) {
		errno = ENAMETOOLONG;
		return (NULL);
	}

	fd = mkstemp(dest);
	if (fd < 0)
		return (NULL);
	fchmod(fd, 0644);	/* nothing secret in the file */
	return (fdopen(fd, "w+"));
}

static char xrefname[MAXPATHLEN], tempname[MAXPATHLEN];

static void
usage(void)
{

	fprintf(stderr, "%s\n",
	    "usage: kldxref [-Rdv] [-f hintsfile] path ..."
	);
	exit(1);
}

static int
#if defined(__GLIBC__) || defined(__APPLE__)
compare(const FTSENT **a, const FTSENT **b)
#else
compare(const FTSENT *const *a, const FTSENT *const *b)
#endif
{

	if ((*a)->fts_info == FTS_D && (*b)->fts_info != FTS_D)
		return (1);
	if ((*a)->fts_info != FTS_D && (*b)->fts_info == FTS_D)
		return (-1);
	return (strcmp((*a)->fts_name, (*b)->fts_name));
}

int
main(int argc, char *argv[])
{
	FTS *ftsp;
	FTSENT *p;
	char *dot = NULL;
	int opt, fts_options;
	struct stat sb;

	fts_options = FTS_PHYSICAL;

	while ((opt = getopt(argc, argv, "Rdf:v")) != -1) {
		switch (opt) {
		case 'd':	/* no hint file, only print on stdout */
			dflag = true;
			break;
		case 'f':	/* use this name instead of linker.hints */
			xref_file = optarg;
			break;
		case 'v':
			verbose++;
			break;
		case 'R':	/* recurse on directories */
			fts_options |= FTS_COMFOLLOW;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}
	if (argc - optind < 1)
		usage();
	argc -= optind;
	argv += optind;

	if (stat(argv[0], &sb) != 0)
		err(1, "%s", argv[0]);
	if ((sb.st_mode & S_IFDIR) == 0 && !dflag) {
		errno = ENOTDIR;
		err(1, "%s", argv[0]);
	}

	if (elf_version(EV_CURRENT) == EV_NONE)
		errx(1, "unsupported libelf");

	ftsp = fts_open(argv, fts_options, compare);
	if (ftsp == NULL)
		exit(1);

	for (;;) {
		p = fts_read(ftsp);
		if ((p == NULL || p->fts_info == FTS_D) && fxref) {
			/* close and rename the current hint file */
			fclose(fxref);
			fxref = NULL;
			if (reccnt != 0) {
				rename(tempname, xrefname);
			} else {
				/* didn't find any entry, ignore this file */
				unlink(tempname);
				unlink(xrefname);
			}
		}
		if (p == NULL)
			break;
		if (p->fts_info == FTS_D && !dflag) {
			/* visiting a new directory, create a new hint file */
			snprintf(xrefname, sizeof(xrefname), "%s/%s",
			    ftsp->fts_path, xref_file);
			fxref = maketempfile(tempname, ftsp->fts_path);
			if (fxref == NULL)
				err(1, "can't create %s", tempname);
			byte_order = ELFDATANONE;
			reccnt = 0;
		}
		/* skip non-files.. */
		if (p->fts_info != FTS_F)
			continue;
		/*
		 * Skip files that generate errors like .debug, .symbol and .pkgsave
		 * by generally skipping all files not ending with ".ko" or that have
		 * no dots in the name (like kernel).
		 */
		dot = strrchr(p->fts_name, '.');
		if (dot != NULL && strcmp(dot, ".ko") != 0)
			continue;
		read_kld(p->fts_path, p->fts_name);
	}
	fts_close(ftsp);
	return (0);
}
