/*
 * Written By Julian ELischer
 * Copyright julian Elischer 1993.
 * Permission is granted to use or redistribute this file in any way as long
 * as this notice remains. Julian Elischer does not guarantee that this file
 * is totally correct for any given task and users of this file must
 * accept responsibility for any damage that occurs from the application of this
 * file.
 *
 * (julian@tfs.com julian@dialix.oz.au)
 *
 * User SCSI hooks added by Peter Dufault:
 *
 * Copyright (c) 1994 HD Associates
 * (contact: dufault@hda.com)
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
 * 3. The name of HD Associates
 *    may not be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY HD ASSOCIATES ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL HD ASSOCIATES BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 * Taken from the original scsi(8) program.
 * from: scsi.c,v 1.17 1998/01/12 07:57:57 charnier Exp $";
 */
#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/file.h>
#include <signal.h>
#include <unistd.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <camlib.h>
#include "camcontrol.h"

int verbose = 0;

/* iget: Integer argument callback
 */
int
iget(void *hook, char *name)
{
	struct get_hook *h = (struct get_hook *)hook;
	int arg;

	if (h->got >= h->argc)
	{
		fprintf(stderr, "Expecting an integer argument.\n");
		usage(0);
		exit(1);
	}
	arg = strtol(h->argv[h->got], 0, 0);
	h->got++;

	if (verbose && name && *name)
		printf("%s: %d\n", name, arg);

	return arg;
}

/* cget: char * argument callback
 */
char *
cget(void *hook, char *name)
{
	struct get_hook *h = (struct get_hook *)hook;
	char *arg;

	if (h->got >= h->argc)
	{
		fprintf(stderr, "Expecting a character pointer argument.\n");
		usage(0);
		exit(1);
	}
	arg = h->argv[h->got];
	h->got++;

	if (verbose && name)
		printf("cget: %s: %s", name, arg);

	return arg;
}

/* arg_put: "put argument" callback
 */
void
arg_put(void *hook, int letter, void *arg, int count, char *name)
{
	if (verbose && name && *name)
		printf("%s:  ", name);

	switch(letter)
	{
		case 'i':
		case 'b':
		printf("%d ", (int)arg);
		break;

		case 'c':
		case 'z':
		{
			char *p;

			p = malloc(count + 1);

			bzero(p, count +1);
			strncpy(p, (char *)arg, count);
			if (letter == 'z')
			{
				int i;
				for (i = count - 1; i >= 0; i--)
					if (p[i] == ' ')
						p[i] = 0;
					else
						break;
			}
			printf("%s ", p);

			free(p);
		}

		break;

		default:
		printf("Unknown format letter: '%c'\n", letter);
	}
	if (verbose)
		putchar('\n');
}

#define START_ENTRY '{'
#define END_ENTRY '}'

static void
skipwhite(FILE *f)
{
	int c;

skip_again:

	while (isspace(c = getc(f)))
		;

	if (c == '#') {
		while ((c = getc(f)) != '\n' && c != EOF)
			;
		goto skip_again;
	}

	ungetc(c, f);
}

/* mode_lookup: Lookup a format description for a given page.
 */
char *mode_db = "/usr/share/misc/scsi_modes";
static char *
mode_lookup(int page)
{
	char *new_db;
	FILE *modes;
	int match, next, found, c;
	static char fmt[4096];	/* XXX This should be with strealloc */
	int page_desc;
	new_db = getenv("SCSI_MODES");

	if (new_db)
		mode_db = new_db;

	modes = fopen(mode_db, "r");
	if (modes == 0)
		return 0;

	next = 0;
	found = 0;

	while (!found) {

		skipwhite(modes);

		if (fscanf(modes, "%i", &page_desc) != 1)
			break;

		if (page_desc == page)
			found = 1;

		skipwhite(modes);
		if (getc(modes) != START_ENTRY)
			errx(1, "expected %c", START_ENTRY);

		match = 1;
		while (match != 0) {
			c = getc(modes);
			if (c == EOF) {
				warnx("expected %c", END_ENTRY);
			}

			if (c == START_ENTRY) {
				match++;
			}
			if (c == END_ENTRY) {
				match--;
				if (match == 0)
					break;
			}
			if (found && c != '\n') {
				if (next >= sizeof(fmt))
					errx(1, "buffer overflow");

				fmt[next++] = (u_char)c;
			}
		}
	}
	fmt[next] = 0;

	return (found) ? fmt : 0;
}

/* -------- edit: Mode Select Editor ---------
 */
struct editinfo
{
	int can_edit;
	int default_value;
} editinfo[64];	/* XXX Bogus fixed size */

static int editind;
volatile int edit_opened;
static FILE *edit_file;
static char edit_name[L_tmpnam];

static inline void
edit_rewind(void)
{
	editind = 0;
}

static void
edit_done(void)
{
	int opened;

	sigset_t all, prev;
	sigfillset(&all);

	(void)sigprocmask(SIG_SETMASK, &all, &prev);

	opened = (int)edit_opened;
	edit_opened = 0;

	(void)sigprocmask(SIG_SETMASK, &prev, 0);

	if (opened)
	{
		if (fclose(edit_file))
			warn("%s", edit_name);
		if (unlink(edit_name))
			warn("%s", edit_name);
	}
}

static void
edit_init(void)
{
	int fd;

	edit_rewind();
	strlcpy(edit_name, "/tmp/camXXXXXX", sizeof(edit_name));
	if ((fd = mkstemp(edit_name)) == -1)
		errx(1, "mkstemp failed");
	if ((edit_file = fdopen(fd, "w")) == 0)
		err(1, "%s", edit_name);
	edit_opened = 1;

	atexit(edit_done);
}

static void
edit_check(void *hook, int letter, void *arg, int count, char *name)
{
	if (letter != 'i' && letter != 'b')
		errx(1, "can't edit format %c", letter);

	if (editind >= sizeof(editinfo) / sizeof(editinfo[0]))
		errx(1, "edit table overflow");

	editinfo[editind].can_edit = ((int)arg != 0);
	editind++;
}

static void
edit_defaults(void *hook, int letter, void *arg, int count, char *name)
{
	if (letter != 'i' && letter != 'b')
		errx(1, "can't edit format %c", letter);

	editinfo[editind].default_value = ((int)arg);
	editind++;
}

static void
edit_report(void *hook, int letter, void *arg, int count, char *name)
{
	if (editinfo[editind].can_edit) {
		if (letter != 'i' && letter != 'b')
			errx(1, "can't report format %c", letter);

		fprintf(edit_file, "%s:  %d\n", name, (int)arg);
	}

	editind++;
}

static int
edit_get(void *hook, char *name)
{
	int arg = editinfo[editind].default_value;

	if (editinfo[editind].can_edit) {
		char line[80];
		if (fgets(line, sizeof(line), edit_file) == 0)
			err(1, "fgets");

		line[strlen(line) - 1] = 0;

		if (strncmp(name, line, strlen(name)) != 0)
			errx(1, "expected \"%s\" and read \"%s\"", name, line);

		arg = strtoul(line + strlen(name) + 2, 0, 0);
	}

	editind++;
	return arg;
}

static void
edit_edit(void)
{
	char *system_line;
	char *editor = getenv("EDITOR");
	if (!editor)
		editor = "vi";

	fclose(edit_file);

	system_line = malloc(strlen(editor) + strlen(edit_name) + 6);
	sprintf(system_line, "%s %s", editor, edit_name);
	system(system_line);
	free(system_line);

	if ((edit_file = fopen(edit_name, "r")) == 0)
		err(1, "%s", edit_name);
}

void
mode_edit(struct cam_device *device, int page, int page_control, int dbd,
	  int edit, int retry_count, int timeout)
{
	int i;
	u_char data[255];
	u_char *mode_pars;
	struct mode_header
	{
		u_char mdl;	/* Mode data length */
		u_char medium_type;
		u_char dev_spec_par;
		u_char bdl;	/* Block descriptor length */
	};

	struct mode_page_header
	{
		u_char page_code;
		u_char page_length;
	};

	struct mode_header *mh;
	struct mode_page_header *mph;

	char *fmt = mode_lookup(page);
	if (!fmt && verbose) {
		fprintf(stderr,
		"No mode data base entry in \"%s\" for page %d; "
		" binary %s only.\n",
		mode_db, page, (edit ? "edit" : "display"));
	}

	if (edit) {
		if (!fmt)
			errx(1, "can't edit without a format");

		if (page_control != 0 && page_control != 3)
			errx(1, "it only makes sense to edit page 0 "
			     "(current) or page 3 (saved values)");

		verbose = 1;

		mode_sense(device, page, 1, dbd, retry_count, timeout,
			   data, sizeof(data));

		mh = (struct mode_header *)data;
		mph = (struct mode_page_header *)
		(((char *)mh) + sizeof(*mh) + mh->bdl);

		mode_pars = (char *)mph + sizeof(*mph);

		edit_init();
		buff_decode_visit(mode_pars, mh->mdl, fmt, edit_check, 0);

		mode_sense(device, page, 0, dbd, retry_count, timeout,
			   data, sizeof(data));

		edit_rewind();
		buff_decode_visit(mode_pars, mh->mdl, fmt, edit_defaults, 0);

		edit_rewind();
		buff_decode_visit(mode_pars, mh->mdl, fmt, edit_report, 0);

		edit_edit();

		edit_rewind();
		buff_encode_visit(mode_pars, mh->mdl, fmt, edit_get, 0);

		/* Eliminate block descriptors:
		 */
		bcopy((char *)mph, ((char *)mh) + sizeof(*mh),
		sizeof(*mph) + mph->page_length);

		mh->bdl = mh->dev_spec_par = 0;
		mph = (struct mode_page_header *) (((char *)mh) + sizeof(*mh));
		mode_pars = ((char *)mph) + 2;

#if 0
		/* Turn this on to see what you're sending to the
		 * device:
		 */
		edit_rewind();
		buff_decode_visit(mode_pars, mh->mdl, fmt, arg_put, 0);
#endif

		edit_done();

		/* Make it permanent if pageselect is three.
		 */

		mph->page_code &= ~0xC0;	/* Clear PS and RESERVED */
		mh->mdl = 0;			/* Reserved for mode select */

		mode_select(device, (page_control == 3), retry_count,
			    timeout, (u_int8_t *)mh, sizeof(*mh) + mh->bdl +
			    sizeof(*mph) + mph->page_length);

		return;
	}

	mode_sense(device, page, page_control, dbd, retry_count, timeout,
		   data, sizeof(data));

	/* Skip over the block descriptors.
	 */
	mh = (struct mode_header *)data;
	mph = (struct mode_page_header *)(((char *)mh) + sizeof(*mh) + mh->bdl);
	mode_pars = (char *)mph + sizeof(*mph);

	if (!fmt) {
		for (i = 0; i < mh->mdl; i++) {
			printf("%02x%c",mode_pars[i],
			       (((i + 1) % 8) == 0) ? '\n' : ' ');
		}
		putc('\n', stdout);
	} else {
		verbose = 1;
		buff_decode_visit(mode_pars, mh->mdl, fmt, arg_put, NULL);
	}
}
