/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2000 Kelly Yancey <kbyanc@posi.net>
 * Derived from work done by Julian Elischer <julian@tfs.com,
 * julian@dialix.oz.au>, 1993, and Peter Dufault <dufault@hda.com>, 1994.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution. 
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

#include <sys/queue.h>
#include <sys/types.h>
#include <sys/sbuf.h>

#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sysexits.h>
#include <unistd.h>

#include <cam/scsi/scsi_all.h>
#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <camlib.h>
#include "camcontrol.h"

#define	DEFAULT_SCSI_MODE_DB	"/usr/share/misc/scsi_modes"
#define	DEFAULT_EDITOR		"vi"
#define	MAX_FORMAT_SPEC		4096	/* Max CDB format specifier. */
#define	MAX_PAGENUM_LEN		10	/* Max characters in page num. */
#define	MAX_PAGENAME_LEN	64	/* Max characters in page name. */
#define	PAGEDEF_START		'{'	/* Page definition delimiter. */
#define	PAGEDEF_END		'}'	/* Page definition delimiter. */
#define	PAGENAME_START		'"'	/* Page name delimiter. */
#define	PAGENAME_END		'"'	/* Page name delimiter. */
#define	PAGEENTRY_END		';'	/* Page entry terminator (optional). */
#define	MAX_DATA_SIZE		4096	/* Mode/Log sense data buffer size. */
#define PAGE_CTRL_SHIFT		6	/* Bit offset to page control field. */

struct editentry {
	STAILQ_ENTRY(editentry) link;
	char	*name;
	char	type;
	int	editable;
	int	size;
	union {
		int	ivalue;
		char	*svalue;
	} value;
};
static STAILQ_HEAD(, editentry) editlist; /* List of page entries. */
static int editlist_changed = 0;	/* Whether any entries were changed. */

struct pagename {
	SLIST_ENTRY(pagename) link;
	int page;
	int subpage;
	char *name;
};
static SLIST_HEAD(, pagename) namelist;	/* Page number to name mappings. */

static char format[MAX_FORMAT_SPEC];	/* Buffer for scsi cdb format def. */

static FILE *edit_file = NULL;		/* File handle for edit file. */
static char edit_path[] = "/tmp/camXXXXXX";


/* Function prototypes. */
static void		 editentry_create(void *hook, int letter, void *arg,
					  int count, char *name);
static void		 editentry_update(void *hook, int letter, void *arg,
					  int count, char *name);
static void		 editentry_create_desc(void *hook, int letter, void *arg,
					  int count, char *name);
static int		 editentry_save(void *hook, char *name);
static struct editentry	*editentry_lookup(char *name);
static int		 editentry_set(char *name, char *newvalue,
				       int editonly);
static void		 editlist_populate(struct cam_device *device,
			    int cdb_len, int dbd, int pc, int page, int subpage,
			    int task_attr, int retries, int timeout);
static void		 editlist_populate_desc(struct cam_device *device,
			    int cdb_len, int llbaa, int pc, int page,
			    int subpage, int task_attr, int retries,
			    int timeout);
static void		 editlist_save(struct cam_device *device, int cdb_len,
			    int dbd, int pc, int page, int subpage,
			    int task_attr, int retries, int timeout);
static void		 editlist_save_desc(struct cam_device *device, int cdb_len,
			    int llbaa, int pc, int page, int subpage,
			    int task_attr, int retries, int timeout);
static void		 nameentry_create(int page, int subpage, char *name);
static struct pagename	*nameentry_lookup(int page, int subpage);
static int		 load_format(const char *pagedb_path, int lpage,
			    int lsubpage);
static int		 modepage_write(FILE *file, int editonly);
static int		 modepage_read(FILE *file);
static void		 modepage_edit(void);
static void		 modepage_dump(struct cam_device *device, int cdb_len,
			    int dbd, int pc, int page, int subpage,
			    int task_attr, int retries, int timeout);
static void		 modepage_dump_desc(struct cam_device *device,
			    int cdb_len, int llbaa, int pc, int page,
			    int subpage, int task_attr, int retries,
			    int timeout);
static void		 cleanup_editfile(void);


#define	returnerr(code) do {						\
	errno = code;							\
	return (-1);							\
} while (0)


#define	RTRIM(string) do {						\
	int _length;							\
	while (isspace(string[_length = strlen(string) - 1]))		\
		string[_length] = '\0';					\
} while (0)


static void
editentry_create(void *hook __unused, int letter, void *arg, int count,
		 char *name)
{
	struct editentry *newentry;	/* Buffer to hold new entry. */

	/* Allocate memory for the new entry and a copy of the entry name. */
	if ((newentry = malloc(sizeof(struct editentry))) == NULL ||
	    (newentry->name = strdup(name)) == NULL)
		err(EX_OSERR, NULL);

	/* Trim any trailing whitespace for the entry name. */
	RTRIM(newentry->name);

	newentry->editable = (arg != NULL);
	newentry->type = letter;
	newentry->size = count;		/* Placeholder; not accurate. */
	newentry->value.svalue = NULL;

	STAILQ_INSERT_TAIL(&editlist, newentry, link);
}

static void
editentry_update(void *hook __unused, int letter, void *arg, int count,
		 char *name)
{
	struct editentry *dest;		/* Buffer to hold entry to update. */

	dest = editentry_lookup(name);
	assert(dest != NULL);

	dest->type = letter;
	dest->size = count;		/* We get the real size now. */

	switch (dest->type) {
	case 'i':			/* Byte-sized integral type. */
	case 'b':			/* Bit-sized integral types. */
	case 't':
		dest->value.ivalue = (intptr_t)arg;
		break;

	case 'c':			/* Character array. */
	case 'z':			/* Null-padded string. */
		editentry_set(name, (char *)arg, 0);
		break;
	default:
		; /* NOTREACHED */
	}
}

static void
editentry_create_desc(void *hook __unused, int letter, void *arg, int count,
		 char *name)
{
	struct editentry *newentry;	/* Buffer to hold new entry. */

	/* Allocate memory for the new entry and a copy of the entry name. */
	if ((newentry = malloc(sizeof(struct editentry))) == NULL ||
	    (newentry->name = strdup(name)) == NULL)
		err(EX_OSERR, NULL);

	/* Trim any trailing whitespace for the entry name. */
	RTRIM(newentry->name);

	newentry->editable = 1;
	newentry->type = letter;
	newentry->size = count;
	newentry->value.svalue = NULL;

	STAILQ_INSERT_TAIL(&editlist, newentry, link);

	switch (letter) {
	case 'i':			/* Byte-sized integral type. */
	case 'b':			/* Bit-sized integral types. */
	case 't':
		newentry->value.ivalue = (intptr_t)arg;
		break;

	case 'c':			/* Character array. */
	case 'z':			/* Null-padded string. */
		editentry_set(name, (char *)arg, 0);
		break;
	default:
		; /* NOTREACHED */
	}
}

static int
editentry_save(void *hook __unused, char *name)
{
	struct editentry *src;		/* Entry value to save. */

	src = editentry_lookup(name);
	if (src == 0) {
		/*
		 * This happens if field does not fit into read page size.
		 * It also means that this field won't be written, so the
		 * returned value does not really matter.
		 */
		return (0);
	}

	switch (src->type) {
	case 'i':			/* Byte-sized integral type. */
	case 'b':			/* Bit-sized integral types. */
	case 't':
		return (src->value.ivalue);
		/* NOTREACHED */

	case 'c':			/* Character array. */
	case 'z':			/* Null-padded string. */
		return ((intptr_t)src->value.svalue);
		/* NOTREACHED */

	default:
		; /* NOTREACHED */
	}

	return (0);			/* This should never happen. */
}

static struct editentry *
editentry_lookup(char *name)
{
	struct editentry *scan;

	assert(name != NULL);

	STAILQ_FOREACH(scan, &editlist, link) {
		if (strcasecmp(scan->name, name) == 0)
			return (scan);
	}

	/* Not found during list traversal. */
	return (NULL);
}

static int
editentry_set(char *name, char *newvalue, int editonly)
{
	struct editentry *dest;	/* Modepage entry to update. */
	char *cval;		/* Pointer to new string value. */
	char *convertend;	/* End-of-conversion pointer. */
	long long ival, newival; /* New integral value. */
	int resolution;		/* Resolution in bits for integer conversion. */

/*
 * Macro to determine the maximum value of the given size for the current
 * resolution.
 */
#define	RESOLUTION_MAX(size)	((1LL << (resolution * (size))) - 1)

	assert(newvalue != NULL);
	if (*newvalue == '\0')
		return (0);	/* Nothing to do. */

	if ((dest = editentry_lookup(name)) == NULL)
		returnerr(ENOENT);
	if (!dest->editable && editonly)
		returnerr(EPERM);

	switch (dest->type) {
	case 'i':		/* Byte-sized integral type. */
	case 'b':		/* Bit-sized integral types. */
	case 't':
		/* Convert the value string to an integer. */
		resolution = (dest->type == 'i')? 8: 1;
		ival = strtoll(newvalue, &convertend, 0);
		if (*convertend != '\0')
			returnerr(EINVAL);
		if (ival > RESOLUTION_MAX(dest->size) || ival < 0) {
			newival = (ival < 0) ? 0 : RESOLUTION_MAX(dest->size);
			warnx("value %lld is out of range for entry %s; "
			    "clipping to %lld", ival, name, newival);
			ival = newival;
		}
		if (dest->value.ivalue != ival)
			editlist_changed = 1;
		dest->value.ivalue = ival;
		break;

	case 'c':		/* Character array. */
	case 'z':		/* Null-padded string. */
		if ((cval = malloc(dest->size + 1)) == NULL)
			err(EX_OSERR, NULL);
		bzero(cval, dest->size + 1);
		strncpy(cval, newvalue, dest->size);
		if (dest->type == 'z') {
			/* Convert trailing spaces to nulls. */
			char *convertend2;

			for (convertend2 = cval + dest->size;
			    convertend2 >= cval; convertend2--) {
				if (*convertend2 == ' ')
					*convertend2 = '\0';
				else if (*convertend2 != '\0')
					break;
			}
		}
		if (strncmp(dest->value.svalue, cval, dest->size) == 0) {
			/* Nothing changed, free the newly allocated string. */
			free(cval);
			break;
		}
		if (dest->value.svalue != NULL) {
			/* Free the current string buffer. */
			free(dest->value.svalue);
			dest->value.svalue = NULL;
		}
		dest->value.svalue = cval;
		editlist_changed = 1;
		break;

	default:
		; /* NOTREACHED */
	}

	return (0);
#undef RESOLUTION_MAX
}

static void
nameentry_create(int page, int subpage, char *name) {
	struct pagename *newentry;

	if (page < 0 || subpage < 0 || name == NULL || name[0] == '\0')
		return;

	/* Allocate memory for the new entry and a copy of the entry name. */
	if ((newentry = malloc(sizeof(struct pagename))) == NULL ||
	    (newentry->name = strdup(name)) == NULL)
		err(EX_OSERR, NULL);

	/* Trim any trailing whitespace for the page name. */
	RTRIM(newentry->name);

	newentry->page = page;
	newentry->subpage = subpage;
	SLIST_INSERT_HEAD(&namelist, newentry, link);
}

static struct pagename *
nameentry_lookup(int page, int subpage) {
	struct pagename *scan;

	SLIST_FOREACH(scan, &namelist, link) {
		if (page == scan->page && subpage == scan->subpage)
			return (scan);
	}

	/* Not found during list traversal. */
	return (NULL);
}

static int
load_format(const char *pagedb_path, int lpage, int lsubpage)
{
	FILE *pagedb;
	char str_page[MAX_PAGENUM_LEN];
	char *str_subpage;
	char str_pagename[MAX_PAGENAME_LEN];
	int page;
	int subpage;
	int depth;			/* Quoting depth. */
	int found;
	int lineno;
	enum { LOCATE, PAGENAME, PAGEDEF } state;
	int ch;
	char c;

#define	SETSTATE_LOCATE do {						\
	str_page[0] = '\0';						\
	str_pagename[0] = '\0';						\
	page = -1;							\
	subpage = -1;							\
	state = LOCATE;							\
} while (0)

#define	SETSTATE_PAGENAME do {						\
	str_pagename[0] = '\0';						\
	state = PAGENAME;						\
} while (0)

#define	SETSTATE_PAGEDEF do {						\
	format[0] = '\0';						\
	state = PAGEDEF;						\
} while (0)

#define	UPDATE_LINENO do {						\
	if (c == '\n')							\
		lineno++;						\
} while (0)

#define	BUFFERFULL(buffer)	(strlen(buffer) + 1 >= sizeof(buffer))

	if ((pagedb = fopen(pagedb_path, "r")) == NULL)
		returnerr(ENOENT);

	SLIST_INIT(&namelist);

	c = '\0';
	depth = 0;
	lineno = 0;
	found = 0;
	SETSTATE_LOCATE;
	while ((ch = fgetc(pagedb)) != EOF) {

		/* Keep a line count to make error messages more useful. */
		UPDATE_LINENO;

		/* Skip over comments anywhere in the mode database. */
		if (ch == '#') {
			do {
				ch = fgetc(pagedb);
			} while (ch != '\n' && ch != EOF);
			UPDATE_LINENO;
			continue;
		}
		c = ch;

		/* Strip out newline characters. */
		if (c == '\n')
			continue;

		/* Keep track of the nesting depth for braces. */
		if (c == PAGEDEF_START)
			depth++;
		else if (c == PAGEDEF_END) {
			depth--;
			if (depth < 0) {
				errx(EX_OSFILE, "%s:%d: %s", pagedb_path,
				    lineno, "mismatched bracket");
			}
		}

		switch (state) {
		case LOCATE:
			/*
			 * Locate the page the user is interested in, skipping
			 * all others.
			 */
			if (isspace(c)) {
				/* Ignore all whitespace between pages. */
				break;
			} else if (depth == 0 && c == PAGEENTRY_END) {
				/*
				 * A page entry terminator will reset page
				 * scanning (useful for assigning names to
				 * modes without providing a mode definition).
				 */
				/* Record the name of this page. */
				str_subpage = str_page;
				strsep(&str_subpage, ",");
				page = strtol(str_page, NULL, 0);
				if (str_subpage)
				    subpage = strtol(str_subpage, NULL, 0);
				else
				    subpage = 0;
				nameentry_create(page, subpage, str_pagename);
				SETSTATE_LOCATE;
			} else if (depth == 0 && c == PAGENAME_START) {
				SETSTATE_PAGENAME;
			} else if (c == PAGEDEF_START) {
				str_subpage = str_page;
				strsep(&str_subpage, ",");
				page = strtol(str_page, NULL, 0);
				if (str_subpage)
				    subpage = strtol(str_subpage, NULL, 0);
				else
				    subpage = 0;
				if (depth == 1) {
					/* Record the name of this page. */
					nameentry_create(page, subpage,
					    str_pagename);
					/*
					 * Only record the format if this is
					 * the page we are interested in.
					 */
					if (lpage == page &&
					    lsubpage == subpage && !found)
						SETSTATE_PAGEDEF;
				}
			} else if (c == PAGEDEF_END) {
				/* Reset the processor state. */
				SETSTATE_LOCATE;
			} else if (depth == 0 && ! BUFFERFULL(str_page)) {
				strncat(str_page, &c, 1);
			} else if (depth == 0) {
				errx(EX_OSFILE, "%s:%d: %s %zd %s", pagedb_path,
				    lineno, "page identifier exceeds",
				    sizeof(str_page) - 1, "characters");
			}
			break;

		case PAGENAME:
			if (c == PAGENAME_END) {
				/*
				 * Return to LOCATE state without resetting the
				 * page number buffer.
				 */
				state = LOCATE;
			} else if (! BUFFERFULL(str_pagename)) {
				strncat(str_pagename, &c, 1);
			} else {
				errx(EX_OSFILE, "%s:%d: %s %zd %s", pagedb_path,
				    lineno, "page name exceeds",
				    sizeof(str_page) - 1, "characters");
			}
			break;

		case PAGEDEF:
			/*
			 * Transfer the page definition into a format buffer
			 * suitable for use with CDB encoding/decoding routines.
			 */
			if (depth == 0) {
				found = 1;
				SETSTATE_LOCATE;
			} else if (! BUFFERFULL(format)) {
				strncat(format, &c, 1);
			} else {
				errx(EX_OSFILE, "%s:%d: %s %zd %s", pagedb_path,
				    lineno, "page definition exceeds",
				    sizeof(format) - 1, "characters");
			}
			break;

		default:
			; /* NOTREACHED */
		}

		/* Repeat processing loop with next character. */
	}

	if (ferror(pagedb))
		err(EX_OSFILE, "%s", pagedb_path);

	/* Close the SCSI page database. */
	fclose(pagedb);

	if (!found)			/* Never found a matching page. */
		returnerr(ESRCH);

	return (0);
}

static void
editlist_populate(struct cam_device *device, int cdb_len, int dbd, int pc,
    int page, int subpage, int task_attr, int retries, int timeout)
{
	u_int8_t data[MAX_DATA_SIZE];	/* Buffer to hold mode parameters. */
	u_int8_t *mode_pars;		/* Pointer to modepage params. */
	struct scsi_mode_page_header *mph;
	struct scsi_mode_page_header_sp *mphsp;
	size_t len;

	STAILQ_INIT(&editlist);

	/* Fetch changeable values; use to build initial editlist. */
	mode_sense(device, &cdb_len, dbd, 0, 1, page, subpage, task_attr,
		   retries, timeout, data, sizeof(data));

	if (cdb_len == 6) {
		struct scsi_mode_header_6 *mh =
		    (struct scsi_mode_header_6 *)data;
		mph = find_mode_page_6(mh);
	} else {
		struct scsi_mode_header_10 *mh =
		    (struct scsi_mode_header_10 *)data;
		mph = find_mode_page_10(mh);
	}
	if ((mph->page_code & SMPH_SPF) == 0) {
		mode_pars = (uint8_t *)(mph + 1);
		len = mph->page_length;
	} else {
		mphsp = (struct scsi_mode_page_header_sp *)mph;
		mode_pars = (uint8_t *)(mphsp + 1);
		len = scsi_2btoul(mphsp->page_length);
	}
	len = MIN(len, sizeof(data) - (mode_pars - data));

	/* Decode the value data, creating edit_entries for each value. */
	buff_decode_visit(mode_pars, len, format, editentry_create, 0);

	/* Fetch the current/saved values; use to set editentry values. */
	mode_sense(device, &cdb_len, dbd, 0, pc, page, subpage, task_attr,
	    retries, timeout, data, sizeof(data));
	buff_decode_visit(mode_pars, len, format, editentry_update, 0);
}

static void
editlist_populate_desc(struct cam_device *device, int cdb_len, int llbaa, int pc,
    int page, int subpage, int task_attr, int retries, int timeout)
{
	uint8_t data[MAX_DATA_SIZE];	/* Buffer to hold mode parameters. */
	uint8_t *desc;			/* Pointer to block descriptor. */
	char num[8];
	struct sbuf sb;
	size_t len;
	u_int longlba, dlen, i;

	STAILQ_INIT(&editlist);

	/* Fetch the current/saved values. */
	mode_sense(device, &cdb_len, 0, llbaa, pc, page, subpage, task_attr,
	    retries, timeout, data, sizeof(data));

	if (cdb_len == 6) {
		struct scsi_mode_header_6 *mh =
		    (struct scsi_mode_header_6 *)data;
		desc = (uint8_t *)(mh + 1);
		len = mh->blk_desc_len;
		longlba = 0;
	} else {
		struct scsi_mode_header_10 *mh =
		    (struct scsi_mode_header_10 *)data;
		desc = (uint8_t *)(mh + 1);
		len = scsi_2btoul(mh->blk_desc_len);
		longlba = (mh->flags & SMH_LONGLBA) != 0;
	}
	dlen = longlba ? 16 : 8;
	len = MIN(len, sizeof(data) - (desc - data));

	sbuf_new(&sb, format, sizeof(format), SBUF_FIXEDLEN);
	num[0] = 0;
	for (i = 0; i * dlen < len; i++) {
		if (i > 0)
			snprintf(num, sizeof(num), " %d", i + 1);
		if (longlba) {
			sbuf_printf(&sb, "{Number of Logical Blocks%s High} i4\n", num);
			sbuf_printf(&sb, "{Number of Logical Blocks%s} i4\n", num);
			sbuf_cat(&sb, "{Reserved} *i4\n");
			sbuf_printf(&sb, "{Logical Block Length%s} i4\n", num);
		} else if (device->pd_type == T_DIRECT) {
			sbuf_printf(&sb, "{Number of Logical Blocks%s} i4\n", num);
			sbuf_cat(&sb, "{Reserved} *i1\n");
			sbuf_printf(&sb, "{Logical Block Length%s} i3\n", num);
		} else {
			sbuf_printf(&sb, "{Density Code%s} i1\n", num);
			sbuf_printf(&sb, "{Number of Logical Blocks%s} i3\n", num);
			sbuf_cat(&sb, "{Reserved} *i1\n");
			sbuf_printf(&sb, "{Logical Block Length%s} i3\n", num);
		}
	}
	sbuf_finish(&sb);
	sbuf_delete(&sb);

	/* Decode the value data, creating edit_entries for each value. */
	buff_decode_visit(desc, len, format, editentry_create_desc, 0);
}

static void
editlist_save(struct cam_device *device, int cdb_len, int dbd, int pc,
    int page, int subpage, int task_attr, int retries, int timeout)
{
	u_int8_t data[MAX_DATA_SIZE];	/* Buffer to hold mode parameters. */
	u_int8_t *mode_pars;		/* Pointer to modepage params. */
	struct scsi_mode_page_header *mph;
	struct scsi_mode_page_header_sp *mphsp;
	size_t len, hlen, mphlen;

	/* Make sure that something changed before continuing. */
	if (! editlist_changed)
		return;

	/* Preload the CDB buffer with the current mode page data. */
	mode_sense(device, &cdb_len, dbd, 0, pc, page, subpage, task_attr,
	    retries, timeout, data, sizeof(data));

	/* Initial headers & offsets. */
	/*
	 * Tape drives include write protect (WP), Buffered Mode and Speed
	 * settings in the device-specific parameter.  Clearing this
	 * parameter on a mode select can have the effect of turning off
	 * write protect or buffered mode, or changing the speed setting of
	 * the tape drive.
	 *
	 * Disks report DPO/FUA support via the device specific parameter
	 * for MODE SENSE, but the bit is reserved for MODE SELECT.  So we
	 * clear this for disks (and other non-tape devices) to avoid
	 * potential errors from the target device.
	 */
	if (cdb_len == 6) {
		struct scsi_mode_header_6 *mh =
		    (struct scsi_mode_header_6 *)data;
		hlen = sizeof(*mh);
		/* Eliminate block descriptors. */
		if (mh->blk_desc_len > 0) {
			bcopy(find_mode_page_6(mh), mh + 1,
			    mh->data_length + 1 - hlen -
			    mh->blk_desc_len);
			mh->blk_desc_len = 0;
		}
		mh->data_length = 0;	/* Reserved for MODE SELECT command. */
		if (device->pd_type != T_SEQUENTIAL)
			mh->dev_spec = 0;	/* See comment above */
		mph = find_mode_page_6(mh);
	} else {
		struct scsi_mode_header_10 *mh =
		    (struct scsi_mode_header_10 *)data;
		hlen = sizeof(*mh);
		/* Eliminate block descriptors. */
		if (scsi_2btoul(mh->blk_desc_len) > 0) {
			bcopy(find_mode_page_10(mh), mh + 1,
			    scsi_2btoul(mh->data_length) + 1 - hlen -
			    scsi_2btoul(mh->blk_desc_len));
			scsi_ulto2b(0, mh->blk_desc_len);
		}
		scsi_ulto2b(0, mh->data_length); /* Reserved for MODE SELECT. */
		if (device->pd_type != T_SEQUENTIAL)
			mh->dev_spec = 0;	/* See comment above */
		mph = find_mode_page_10(mh);
	}
	if ((mph->page_code & SMPH_SPF) == 0) {
		mphlen = sizeof(*mph);
		mode_pars = (uint8_t *)(mph + 1);
		len = mph->page_length;
	} else {
		mphsp = (struct scsi_mode_page_header_sp *)mph;
		mphlen = sizeof(*mphsp);
		mode_pars = (uint8_t *)(mphsp + 1);
		len = scsi_2btoul(mphsp->page_length);
	}
	len = MIN(len, sizeof(data) - (mode_pars - data));

	/* Encode the value data to be passed back to the device. */
	buff_encode_visit(mode_pars, len, format, editentry_save, 0);

	mph->page_code &= ~SMPH_PS;	/* Reserved for MODE SELECT command. */

	/*
	 * Write the changes back to the device. If the user editted control
	 * page 3 (saved values) then request the changes be permanently
	 * recorded.
	 */
	mode_select(device, cdb_len, (pc << PAGE_CTRL_SHIFT == SMS_PAGE_CTRL_SAVED),
	    task_attr, retries, timeout, data, hlen + mphlen + len);
}

static void
editlist_save_desc(struct cam_device *device, int cdb_len, int llbaa, int pc,
    int page, int subpage, int task_attr, int retries, int timeout)
{
	uint8_t data[MAX_DATA_SIZE];	/* Buffer to hold mode parameters. */
	uint8_t *desc;			/* Pointer to block descriptor. */
	size_t len, hlen;

	/* Make sure that something changed before continuing. */
	if (! editlist_changed)
		return;

	/* Preload the CDB buffer with the current mode page data. */
	mode_sense(device, &cdb_len, 0, llbaa, pc, page, subpage, task_attr,
	    retries, timeout, data, sizeof(data));

	/* Initial headers & offsets. */
	if (cdb_len == 6) {
		struct scsi_mode_header_6 *mh =
		    (struct scsi_mode_header_6 *)data;
		hlen = sizeof(*mh);
		desc = (uint8_t *)(mh + 1);
		len = mh->blk_desc_len;
		mh->data_length = 0;	/* Reserved for MODE SELECT command. */
		if (device->pd_type != T_SEQUENTIAL)
			mh->dev_spec = 0;	/* See comment above */
	} else {
		struct scsi_mode_header_10 *mh =
		    (struct scsi_mode_header_10 *)data;
		hlen = sizeof(*mh);
		desc = (uint8_t *)(mh + 1);
		len = scsi_2btoul(mh->blk_desc_len);
		scsi_ulto2b(0, mh->data_length); /* Reserved for MODE SELECT. */
		if (device->pd_type != T_SEQUENTIAL)
			mh->dev_spec = 0;	/* See comment above */
	}
	len = MIN(len, sizeof(data) - (desc - data));

	/* Encode the value data to be passed back to the device. */
	buff_encode_visit(desc, len, format, editentry_save, 0);

	/*
	 * Write the changes back to the device. If the user editted control
	 * page 3 (saved values) then request the changes be permanently
	 * recorded.
	 */
	mode_select(device, cdb_len, (pc << PAGE_CTRL_SHIFT == SMS_PAGE_CTRL_SAVED),
	    task_attr, retries, timeout, data, hlen + len);
}

static int
modepage_write(FILE *file, int editonly)
{
	struct editentry *scan;
	int written = 0;

	STAILQ_FOREACH(scan, &editlist, link) {
		if (scan->editable || !editonly) {
			written++;
			if (scan->type == 'c' || scan->type == 'z') {
				fprintf(file, "%s:  %s\n", scan->name,
				    scan->value.svalue);
			} else {
				fprintf(file, "%s:  %u\n", scan->name,
				    scan->value.ivalue);
			}
		}
	}
	return (written);
}

static int
modepage_read(FILE *file)
{
	char *buffer;			/* Pointer to dynamic line buffer.  */
	char *line;			/* Pointer to static fgetln buffer. */
	char *name;			/* Name portion of the line buffer. */
	char *value;			/* Value portion of line buffer.    */
	size_t length;			/* Length of static fgetln buffer.  */

#define	ABORT_READ(message, param) do {					\
	warnx(message, param);						\
	free(buffer);							\
	returnerr(EAGAIN);						\
} while (0)

	while ((line = fgetln(file, &length)) != NULL) {
		/* Trim trailing whitespace (including optional newline). */
		while (length > 0 && isspace(line[length - 1]))
			length--;

	    	/* Allocate a buffer to hold the line + terminating null. */
	    	if ((buffer = malloc(length + 1)) == NULL)
			err(EX_OSERR, NULL);
		memcpy(buffer, line, length);
		buffer[length] = '\0';

		/* Strip out comments. */
		if ((value = strchr(buffer, '#')) != NULL)
			*value = '\0';

		/* The name is first in the buffer. Trim whitespace.*/
		name = buffer;
		RTRIM(name);
		while (isspace(*name))
			name++;

		/* Skip empty lines. */
		if (strlen(name) == 0)
			continue;

		/* The name ends at the colon; the value starts there. */
		if ((value = strrchr(buffer, ':')) == NULL)
			ABORT_READ("no value associated with %s", name);
		*value = '\0';			/* Null-terminate name. */
		value++;			/* Value starts afterwards. */

		/* Trim leading and trailing whitespace. */
		RTRIM(value);
		while (isspace(*value))
			value++;

		/* Make sure there is a value left. */
		if (strlen(value) == 0)
			ABORT_READ("no value associated with %s", name);

		/* Update our in-memory copy of the modepage entry value. */
		if (editentry_set(name, value, 1) != 0) {
			if (errno == ENOENT) {
				/* No entry by the name. */
				ABORT_READ("no such modepage entry \"%s\"",
				    name);
			} else if (errno == EINVAL) {
				/* Invalid value. */
				ABORT_READ("Invalid value for entry \"%s\"",
				    name);
			} else if (errno == ERANGE) {
				/* Value out of range for entry type. */
				ABORT_READ("value out of range for %s", name);
			} else if (errno == EPERM) {
				/* Entry is not editable; not fatal. */
				warnx("modepage entry \"%s\" is read-only; "
				    "skipping.", name);
			}
		}

		free(buffer);
	}
	return (ferror(file)? -1: 0);

#undef ABORT_READ
}

static void
modepage_edit(void)
{
	const char *editor;
	char *commandline;
	int fd;
	int written;

	if (!isatty(fileno(stdin))) {
		/* Not a tty, read changes from stdin. */
		modepage_read(stdin);
		return;
	}

	/* Lookup editor to invoke. */
	if ((editor = getenv("EDITOR")) == NULL)
		editor = DEFAULT_EDITOR;

	/* Create temp file for editor to modify. */
	if ((fd = mkstemp(edit_path)) == -1)
		errx(EX_CANTCREAT, "mkstemp failed");

	atexit(cleanup_editfile);

	if ((edit_file = fdopen(fd, "w")) == NULL)
		err(EX_NOINPUT, "%s", edit_path);

	written = modepage_write(edit_file, 1);

	fclose(edit_file);
	edit_file = NULL;

	if (written == 0) {
		warnx("no editable entries");
		cleanup_editfile();
		return;
	}

	/*
	 * Allocate memory to hold the command line (the 2 extra characters
	 * are to hold the argument separator (a space), and the terminating
	 * null character.
	 */
	commandline = malloc(strlen(editor) + strlen(edit_path) + 2);
	if (commandline == NULL)
		err(EX_OSERR, NULL);
	sprintf(commandline, "%s %s", editor, edit_path);

	/* Invoke the editor on the temp file. */
	if (system(commandline) == -1)
		err(EX_UNAVAILABLE, "could not invoke %s", editor);
	free(commandline);

	if ((edit_file = fopen(edit_path, "r")) == NULL)
		err(EX_NOINPUT, "%s", edit_path);

	/* Read any changes made to the temp file. */
	modepage_read(edit_file);

	cleanup_editfile();
}

static void
modepage_dump(struct cam_device *device, int cdb_len, int dbd, int pc,
	      int page, int subpage, int task_attr, int retries, int timeout)
{
	u_int8_t data[MAX_DATA_SIZE];	/* Buffer to hold mode parameters. */
	u_int8_t *mode_pars;		/* Pointer to modepage params. */
	struct scsi_mode_page_header *mph;
	struct scsi_mode_page_header_sp *mphsp;
	size_t indx, len;

	mode_sense(device, &cdb_len, dbd, 0, pc, page, subpage, task_attr,
	    retries, timeout, data, sizeof(data));

	if (cdb_len == 6) {
		struct scsi_mode_header_6 *mh =
		    (struct scsi_mode_header_6 *)data;
		mph = find_mode_page_6(mh);
	} else {
		struct scsi_mode_header_10 *mh =
		    (struct scsi_mode_header_10 *)data;
		mph = find_mode_page_10(mh);
	}
	if ((mph->page_code & SMPH_SPF) == 0) {
		mode_pars = (uint8_t *)(mph + 1);
		len = mph->page_length;
	} else {
		mphsp = (struct scsi_mode_page_header_sp *)mph;
		mode_pars = (uint8_t *)(mphsp + 1);
		len = scsi_2btoul(mphsp->page_length);
	}
	len = MIN(len, sizeof(data) - (mode_pars - data));

	/* Print the raw mode page data with newlines each 8 bytes. */
	for (indx = 0; indx < len; indx++) {
		printf("%02x%c",mode_pars[indx],
		    (((indx + 1) % 8) == 0) ? '\n' : ' ');
	}
	putchar('\n');
}
static void
modepage_dump_desc(struct cam_device *device, int cdb_len, int llbaa, int pc,
	      int page, int subpage, int task_attr, int retries, int timeout)
{
	uint8_t data[MAX_DATA_SIZE];	/* Buffer to hold mode parameters. */
	uint8_t *desc;			/* Pointer to block descriptor. */
	size_t indx, len;

	mode_sense(device, &cdb_len, 0, llbaa, pc, page, subpage, task_attr,
	    retries, timeout, data, sizeof(data));

	if (cdb_len == 6) {
		struct scsi_mode_header_6 *mh =
		    (struct scsi_mode_header_6 *)data;
		desc = (uint8_t *)(mh + 1);
		len = mh->blk_desc_len;
	} else {
		struct scsi_mode_header_10 *mh =
		    (struct scsi_mode_header_10 *)data;
		desc = (uint8_t *)(mh + 1);
		len = scsi_2btoul(mh->blk_desc_len);
	}
	len = MIN(len, sizeof(data) - (desc - data));

	/* Print the raw mode page data with newlines each 8 bytes. */
	for (indx = 0; indx < len; indx++) {
		printf("%02x%c", desc[indx],
		    (((indx + 1) % 8) == 0) ? '\n' : ' ');
	}
	putchar('\n');
}

static void
cleanup_editfile(void)
{
	if (edit_file == NULL)
		return;
	if (fclose(edit_file) != 0 || unlink(edit_path) != 0)
		warn("%s", edit_path);
	edit_file = NULL;
}

void
mode_edit(struct cam_device *device, int cdb_len, int desc, int dbd, int llbaa,
    int pc, int page, int subpage, int edit, int binary, int task_attr,
    int retry_count, int timeout)
{
	const char *pagedb_path;	/* Path to modepage database. */

	if (binary) {
		if (edit)
			errx(EX_USAGE, "cannot edit in binary mode.");
	} else if (desc) {
		editlist_populate_desc(device, cdb_len, llbaa, pc, page,
		    subpage, task_attr, retry_count, timeout);
	} else {
		if ((pagedb_path = getenv("SCSI_MODES")) == NULL)
			pagedb_path = DEFAULT_SCSI_MODE_DB;

		if (load_format(pagedb_path, page, subpage) != 0 &&
		    (edit || verbose)) {
			if (errno == ENOENT) {
				/* Modepage database file not found. */
				warn("cannot open modepage database \"%s\"",
				    pagedb_path);
			} else if (errno == ESRCH) {
				/* Modepage entry not found in database. */
				warnx("modepage 0x%02x,0x%02x not found in "
				    "database \"%s\"", page, subpage,
				    pagedb_path);
			}
			/* We can recover in display mode, otherwise we exit. */
			if (!edit) {
				warnx("reverting to binary display only");
				binary = 1;
			} else
				exit(EX_OSFILE);
		}

		editlist_populate(device, cdb_len, dbd, pc, page, subpage,
		    task_attr, retry_count, timeout);
	}

	if (edit) {
		if (pc << PAGE_CTRL_SHIFT != SMS_PAGE_CTRL_CURRENT &&
		    pc << PAGE_CTRL_SHIFT != SMS_PAGE_CTRL_SAVED)
			errx(EX_USAGE, "it only makes sense to edit page 0 "
			    "(current) or page 3 (saved values)");
		modepage_edit();
		if (desc) {
			editlist_save_desc(device, cdb_len, llbaa, pc, page,
			    subpage, task_attr, retry_count, timeout);
		} else {
			editlist_save(device, cdb_len, dbd, pc, page, subpage,
			    task_attr, retry_count, timeout);
		}
	} else if (binary || STAILQ_EMPTY(&editlist)) {
		/* Display without formatting information. */
		if (desc) {
			modepage_dump_desc(device, cdb_len, llbaa, pc, page,
			    subpage, task_attr, retry_count, timeout);
		} else {
			modepage_dump(device, cdb_len, dbd, pc, page, subpage,
			    task_attr, retry_count, timeout);
		}
	} else {
		/* Display with format. */
		modepage_write(stdout, 0);
	}
}

void
mode_list(struct cam_device *device, int cdb_len, int dbd, int pc, int subpages,
	  int task_attr, int retry_count, int timeout)
{
	u_int8_t data[MAX_DATA_SIZE];	/* Buffer to hold mode parameters. */
	struct scsi_mode_page_header *mph;
	struct scsi_mode_page_header_sp *mphsp;
	struct pagename *nameentry;
	const char *pagedb_path;
	int len, off, page, subpage;

	if ((pagedb_path = getenv("SCSI_MODES")) == NULL)
		pagedb_path = DEFAULT_SCSI_MODE_DB;

	if (load_format(pagedb_path, 0, 0) != 0 && verbose && errno == ENOENT) {
		/* Modepage database file not found. */
		warn("cannot open modepage database \"%s\"", pagedb_path);
	}

	/* Build the list of all mode pages by querying the "all pages" page. */
	mode_sense(device, &cdb_len, dbd, 0, pc, SMS_ALL_PAGES_PAGE,
	    subpages ? SMS_SUBPAGE_ALL : 0,
	    task_attr, retry_count, timeout, data, sizeof(data));

	/* Skip block descriptors. */
	if (cdb_len == 6) {
		struct scsi_mode_header_6 *mh =
		    (struct scsi_mode_header_6 *)data;
		len = mh->data_length;
		off = sizeof(*mh) + mh->blk_desc_len;
	} else {
		struct scsi_mode_header_10 *mh =
		    (struct scsi_mode_header_10 *)data;
		len = scsi_2btoul(mh->data_length);
		off = sizeof(*mh) + scsi_2btoul(mh->blk_desc_len);
	}
	/* Iterate through the pages in the reply. */
	while (off < len) {
		/* Locate the next mode page header. */
		mph = (struct scsi_mode_page_header *)(data + off);

		if ((mph->page_code & SMPH_SPF) == 0) {
			page = mph->page_code & SMS_PAGE_CODE;
			subpage = 0;
			off += sizeof(*mph) + mph->page_length;
		} else {
			mphsp = (struct scsi_mode_page_header_sp *)mph;
			page = mphsp->page_code & SMS_PAGE_CODE;
			subpage = mphsp->subpage;
			off += sizeof(*mphsp) + scsi_2btoul(mphsp->page_length);
		}

		nameentry = nameentry_lookup(page, subpage);
		if (subpage == 0) {
			printf("0x%02x\t%s\n", page,
			    nameentry ? nameentry->name : "");
		} else {
			printf("0x%02x,0x%02x\t%s\n", page, subpage,
			    nameentry ? nameentry->name : "");
		}
	}
}
