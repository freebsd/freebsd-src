/*-
 * Copyright (c) 2006 Michael Bushkov <bushman@freebsd.org>
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "testutil.h"

enum test_methods {
	TEST_GETUSERSHELL,
	TEST_BUILD_SNAPSHOT
};

struct usershell {
	char *path;
};

static int debug = 0;
static enum test_methods method = TEST_GETUSERSHELL;

DECLARE_TEST_DATA(usershell)
DECLARE_TEST_FILE_SNAPSHOT(usershell)
DECLARE_2PASS_TEST(usershell)

static void clone_usershell(struct usershell *, struct usershell const *);
static int compare_usershell(struct usershell *, struct usershell *, void *);
static void free_usershell(struct usershell *);

static void sdump_usershell(struct usershell *, char *, size_t);
static void dump_usershell(struct usershell *);

static int usershell_read_snapshot_func(struct usershell *, char *);

static void usage(void)  __attribute__((__noreturn__));

IMPLEMENT_TEST_DATA(usershell)
IMPLEMENT_TEST_FILE_SNAPSHOT(usershell)
IMPLEMENT_2PASS_TEST(usershell)

static void 
clone_usershell(struct usershell *dest, struct usershell const *src)
{
	assert(dest != NULL);
	assert(src != NULL);
	
	if (src->path != NULL) {
		dest->path = strdup(src->path);
		assert(dest->path != NULL);
	}
}

static int 
compare_usershell(struct usershell *us1, struct usershell *us2, void *mdata)
{
	int rv;
	
	assert(us1 != NULL);
	assert(us2 != NULL);
	
	dump_usershell(us1);
	dump_usershell(us2);
	
	if (us1 == us2)
		return (0);

	rv = strcmp(us1->path, us2->path);
	if (rv != 0) {
		printf("following structures are not equal:\n");
		dump_usershell(us1);
		dump_usershell(us2);
	}
	
	return (rv);
}

static void 
free_usershell(struct usershell *us)
{
	free(us->path);
}

static void 
sdump_usershell(struct usershell *us, char *buffer, size_t buflen)
{
	snprintf(buffer, buflen, "%s", us->path);
}

static void
dump_usershell(struct usershell *us)
{
	if (us != NULL) {
		char buffer[2048];
		sdump_usershell(us, buffer, sizeof(buffer));
		printf("%s\n", buffer);
	} else
		printf("(null)\n");
}

static int 
usershell_read_snapshot_func(struct usershell *us, char *line)
{
	us->path = strdup(line);
	assert(us->path != NULL);
	
	return (0);
}

static void
usage(void)
{
	(void)fprintf(stderr,
	    "Usage: %s [-d] -s <file>\n",
	    getprogname());
	exit(1);
}

int
main(int argc, char **argv)
{
	struct usershell_test_data td, td_snap;
	struct usershell ushell;
	char *snapshot_file;
	int rv;
	int c;
	
	if (argc < 2)
		usage();

	rv = 0;
	snapshot_file = NULL;
	while ((c = getopt(argc, argv, "ds:")) != -1) {
		switch (c) {
		case 'd':
			debug = 1;
			break;
		case 's':
			snapshot_file = strdup(optarg);
			break;
		default:
			usage();
		}
	}
	
	TEST_DATA_INIT(usershell, &td, clone_usershell, free_usershell);
	TEST_DATA_INIT(usershell, &td_snap, clone_usershell, free_usershell);
			
	setusershell();
	while ((ushell.path = getusershell()) != NULL) {
		if (debug) {
			printf("usershell found:\n");
			dump_usershell(&ushell);
		}
		TEST_DATA_APPEND(usershell, &td, &ushell);
	}
	endusershell();
	
	
	if (snapshot_file != NULL) {
		if (access(snapshot_file, W_OK | R_OK) != 0) {		
			if (errno == ENOENT)
				method = TEST_BUILD_SNAPSHOT;
			else {
				if (debug)
				    printf("can't access the snapshot file %s\n",
				    snapshot_file);
			
				rv = -1;
				goto fin;
			}
		} else {
			rv = TEST_SNAPSHOT_FILE_READ(usershell, snapshot_file,
				&td_snap, usershell_read_snapshot_func);
			if (rv != 0) {
				if (debug)
					printf("error reading snapshot file\n");
				goto fin;
			}
		}
	}
		
	switch (method) {
	case TEST_GETUSERSHELL:
		if (snapshot_file != NULL) {
			rv = DO_2PASS_TEST(usershell, &td, &td_snap,
				compare_usershell, NULL);
		}
		break;
	case TEST_BUILD_SNAPSHOT:
		if (snapshot_file != NULL) {
		    rv = TEST_SNAPSHOT_FILE_WRITE(usershell, snapshot_file, &td, 
			sdump_usershell);
		}
		break;
	default:
		rv = 0;
		break;
	};

fin:
	TEST_DATA_DESTROY(usershell, &td_snap);
	TEST_DATA_DESTROY(usershell, &td);
	free(snapshot_file);
	return (rv);

}
