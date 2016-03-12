/*	$Id: test-sqlite3.c,v 1.2 2015/10/06 18:32:20 schwarze Exp $	*/
/*
 * Copyright (c) 2014 Ingo Schwarze <schwarze@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdio.h>
#include <unistd.h>
#include <sqlite3.h>

int
main(void)
{
	sqlite3	*db;

	if (sqlite3_open_v2("test.db", &db,
	    SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
	    NULL) != SQLITE_OK) {
		perror("test.db");
		fprintf(stderr, "sqlite3_open_v2: %s", sqlite3_errmsg(db));
		return 1;
	}
	unlink("test.db");

	if (sqlite3_exec(db, "PRAGMA foreign_keys = ON",
	    NULL, NULL, NULL) != SQLITE_OK) {
		fprintf(stderr, "sqlite3_exec: %s", sqlite3_errmsg(db));
		return 1;
	}

	if (sqlite3_close(db) != SQLITE_OK) {
		fprintf(stderr, "sqlite3_close: %s", sqlite3_errmsg(db));
		return 1;
	}
	return 0;
}
