/*
 * Copyright (c) 1996, 1997, 1998 Shigio Yamaguchi. All rights reserved.
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
 *      This product includes software developed by Shigio Yamaguchi.
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
 *
 *	pathop.c				12-Nov-98
 *
 */
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "die.h"
#include "dbop.h"
#include "makepath.h"
#include "pathop.h"

static DBOP	*dbop;
static const char *gpath = "GPATH";
static int	_nextkey;
static int	_mode;
static int	opened;
static int	created;

/*
 * pathopen: open path dictionary tag.
 *
 *	i)	mode	0: read only
 *			1: create
 *			2: modify
 *	r)		0: normal
 *			-1: error
 */
int
pathopen(dbpath, mode)
char	*dbpath;
int	mode;
{
	char	*p;

	assert(opened == 0);
	/*
	 * We create GPATH just first time.
	 */
	_mode = mode;
	if (mode == 1 && created)
		mode = 0;
	dbop = dbop_open(makepath(dbpath, gpath), mode, 0644, 0);
	if (dbop == NULL)
		return -1;
	if (mode == 1)
		_nextkey = 0;
	else {
		if (!(p = dbop_get(dbop, NEXTKEY)))
			die("nextkey not found in GPATH.");
		_nextkey = atoi(p);
	}
	opened = 1;
	return 0;
}
void
pathput(path)
char	*path;
{
	char	buf[10];

	assert(opened == 1);
	if (_mode == 1 && created)
		return;
	if (dbop_get(dbop, path) != NULL)
		return;
	sprintf(buf, "%d", _nextkey++);
	dbop_put(dbop, path, buf);
	dbop_put(dbop, buf, path);
}
char	*
pathget(key)
char	*key;
{
	assert(opened == 1);
	return dbop_get(dbop, key);
}
char	*
pathiget(n)
int	n;
{
	char	key[80];
	assert(opened == 1);
	sprintf(key, "%d", n);
	return dbop_get(dbop, key);
}
void
pathdel(key)
char	*key;
{
	char	*d;

	assert(opened == 1);
	assert(_mode == 2);
	assert(key[0] == '.' && key[1] == '/');
	d = dbop_get(dbop, key);
	if (d == NULL)
		return;
	dbop_del(dbop, d);
	dbop_del(dbop, key);
}
int
nextkey(void)
{
	assert(_mode != 1);
	return _nextkey;
}
void
pathclose(void)
{
	char	buf[10];

	assert(opened == 1);
	opened = 0;
	if (_mode == 1 && created) {
		dbop_close(dbop);
		return;
	}
	sprintf(buf, "%d", _nextkey);
	if (_mode == 1 || _mode == 2)
		dbop_put(dbop, NEXTKEY, buf);
	dbop_close(dbop);
	if (_mode == 1)
		created = 1;
}
