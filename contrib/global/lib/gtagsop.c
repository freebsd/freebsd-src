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
 *	gtagsop.c				12-Nov-98
 *
 */
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dbop.h"
#include "die.h"
#include "gtagsop.h"
#include "locatestring.h"
#include "makepath.h"
#include "mgets.h"
#include "pathop.h"
#include "strbuf.h"
#include "strmake.h"
#include "tab.h"

static char	*genrecord __P((GTOP *));
static int	belongto __P((GTOP *, char *, char *));

static int	support_version = 2;	/* acceptable format version   */
static const char *tagslist[] = {"GTAGS", "GRTAGS", "GSYMS"};
/*
 * dbname: return db name
 *
 *	i)	db	0: GTAGS, 1: GRTAGS, 2: GSYMS
 *	r)		dbname
 */
const char *
dbname(db)
int	db;
{
	assert(db >= 0 && db < GTAGLIM);
	return tagslist[db];
}
/*
 * makecommand: make command line to make global tag file
 *
 *	i)	comline	skelton command line
 *	i)	path	path name
 *	o)	sb	command line
 */
void
makecommand(comline, path, sb)
char	*comline;
char	*path;
STRBUF	*sb;
{
	char	*p;

	if (!(p = strmake(comline, "%")))
		die1("'%%s' is needed in tag command line. (%s)\n", comline);
	strputs(sb, p);
	strputs(sb, path);
	if (!(p = locatestring(comline, "%s", MATCH_FIRST)))
		die1("'%%s' is needed in tag command line. (%s)\n", comline);
	strputs(sb, p+2);
}
/*
 * formatcheck: check format of tag command's output
 *
 *	i)	line	input
 *	i)	flags	flag
 *	r)	0:	normal
 *		-1:	tag name
 *		-2:	line number
 *		-3:	path
 *
 * [example of right format]
 *
 * $1                $2 $3             $4
 * ----------------------------------------------------
 * main              83 ./ctags.c        main(argc, argv)
 */
int
formatcheck(line, flags)
char	*line;
int	flags;
{
	char	*p, *q;
	/*
	 * $1 = tagname: allowed any char except sepalator.
	 */
	p = q = line;
	while (*p && !isspace(*p))
		p++;
	while (*p && isspace(*p))
		p++;
	if (p == q)
		return -1;
	/*
	 * $2 = line number: must be digit.
	 */
	q = p;
	while (*p && !isspace(*p))
		if (!isdigit(*p))
			return -2;
		else
			p++;
	if (p == q)
		return -2;
	while (*p && isspace(*p))
		p++;
	/*
	 * $3 = path:
	 *	standard format: must start with './'.
	 *	compact format: must be digit.
	 */
	if (flags & GTAGS_PATHINDEX) {
		while (*p && !isspace(*p))
			if (!isdigit(*p))
				return -3;
			else
				p++;
	} else {
		if (!(*p == '.' && *(p + 1) == '/' && *(p + 2)))
			return -3;
	}
	return 0;
}
/*
 * gtagsopen: open global tag.
 *
 *	i)	dbpath	dbpath directory
 *	i)	root	root directory (needed when compact format)
 *	i)	db	GTAGS, GRTAGS, GSYMS
 *	i)	mode	GTAGS_READ: read only
 *			GTAGS_CREATE: create tag
 *			GTAGS_MODIFY: modify tag
 *	i)	flags	GTAGS_COMPACT
 *			GTAGS_PATHINDEX
 *	r)		GTOP structure
 *
 * when error occurred, gtagopen doesn't return.
 * GTAGS_PATHINDEX needs GTAGS_COMPACT.
 */
GTOP	*
gtagsopen(dbpath, root, db, mode, flags)
char	*dbpath;
char	*root;
int	db;
int	mode;
int	flags;
{
	GTOP	*gtop;
	int	dbmode = 0;

	if ((gtop = (GTOP *)calloc(sizeof(GTOP), 1)) == NULL)
		die("short of memory.");
	gtop->db = db;
	gtop->mode = mode;
	switch (gtop->mode) {
	case GTAGS_READ:
		dbmode = 0;
		break;
	case GTAGS_CREATE:
		dbmode = 1;
		break;
	case GTAGS_MODIFY:
		dbmode = 2;
		break;
	default:
		assert(0);
	}

	/*
	 * allow duplicate records.
	 */
	gtop->dbop = dbop_open(makepath(dbpath, dbname(db)), dbmode, 0644, DBOP_DUP);
	if (gtop->dbop == NULL) {
		if (dbmode == 1)
			die1("cannot make %s.", dbname(db));
		die1("%s not found.", dbname(db));
	}
	/*
	 * decide format version.
	 */
	gtop->format_version = 1;
	gtop->format = GTAGS_STANDARD;
	/*
	 * This is a special case. GSYMS had compact format even if
	 * format version 1.
	 */
	if (db == GSYMS)
		gtop->format |= GTAGS_COMPACT;
	if (gtop->mode == GTAGS_CREATE) {
		if (flags & GTAGS_COMPACT) {
			char	buf[80];

			gtop->format_version = 2;
			sprintf(buf, "%s %d", VERSIONKEY, gtop->format_version);
			dbop_put(gtop->dbop, VERSIONKEY, buf);
			gtop->format |= GTAGS_COMPACT;
			dbop_put(gtop->dbop, COMPACTKEY, COMPACTKEY);
			if (flags & GTAGS_PATHINDEX) {
				gtop->format |= GTAGS_PATHINDEX;
				dbop_put(gtop->dbop, PATHINDEXKEY, PATHINDEXKEY);
			}
		}
	} else {
		/*
		 * recognize format version of GTAGS. 'format version record'
		 * is saved as a META record in GTAGS and GRTAGS.
		 * if 'format version record' is not found, it's assumed
		 * version 1.
		 */
		char	*p;

		if ((p = dbop_get(gtop->dbop, VERSIONKEY)) != NULL) {
			for (p += strlen(VERSIONKEY); *p && isspace(*p); p++)
				;
			gtop->format_version = atoi(p);
		}
		if (gtop->format_version > support_version)
			die("GTAGS seems new format. Please install the latest GLOBAL.");
		if (gtop->format_version > 1) {
			if (dbop_get(gtop->dbop, COMPACTKEY) != NULL)
				gtop->format |= GTAGS_COMPACT;
			if (dbop_get(gtop->dbop, PATHINDEXKEY) != NULL)
				gtop->format |= GTAGS_PATHINDEX;
		}
	}
	if (gtop->format & GTAGS_PATHINDEX || gtop->mode != GTAGS_READ) {
		if (pathopen(dbpath, dbmode) < 0) {
			if (dbmode == 1)
				die("cannot create GPATH.");
			else
				die("GPATH not found.");
		}
	}
	/*
	 * Stuff for compact format.
	 */
	if (gtop->format & GTAGS_COMPACT) {
		assert(root != NULL);
		strcpy(gtop->root, root);
		if (gtop->mode != GTAGS_READ)
			gtop->sb = stropen();
	}
	return gtop;
}
/*
 * gtagsput: put tag record with packing.
 *
 *	i)	gtop	descripter of GTOP
 *	i)	tag	tag name
 *	i)	record	ctags -x image
 */
void
gtagsput(gtop, tag, record)
GTOP	*gtop;
char	*tag;
char	*record;
{
	char	*p, *q;
	char	lno[10];
	char	path[MAXPATHLEN+1];

	if (gtop->format == GTAGS_STANDARD) {
		entab(record);
		dbop_put(gtop->dbop, tag, record);
		return;
	}
	/*
	 * gtop->format & GTAGS_COMPACT
	 */
	p = record;				/* ignore $1 */
	while (*p && !isspace(*p))
		p++;
	while (*p && isspace(*p))
		p++;
	q = lno;				/* lno = $2 */
	while (*p && !isspace(*p))
		*q++ = *p++;
	*q = 0;
	while (*p && isspace(*p))
		p++;
	q = path;				/* path = $3 */
	while (*p && !isspace(*p))
		*q++ = *p++;
	*q = 0;
	/*
	 * First time, it occurs, because 'prev_tag' and 'prev_path' are NULL.
	 */
	if (strcmp(gtop->prev_tag, tag) || strcmp(gtop->prev_path, path)) {
		if (gtop->prev_tag[0])
			dbop_put(gtop->dbop, gtop->prev_tag, strvalue(gtop->sb));
		strcpy(gtop->prev_tag, tag);
		strcpy(gtop->prev_path, path);
		/*
		 * Start creating new record.
		 */
		strstart(gtop->sb);
		strputs(gtop->sb, strmake(record, " \t"));
		strputc(gtop->sb, ' ');
		strputs(gtop->sb, path);
		strputc(gtop->sb, ' ');
		strputs(gtop->sb, lno);
	} else {
		strputc(gtop->sb, ',');
		strputs(gtop->sb, lno);
	}
}
/*
 * gtagsadd: add tags belonging to the path into tag file.
 *
 *	i)	gtop	descripter of GTOP
 *	i)	comline	tag command line
 *	i)	path	source file
 *	i)	flags	GTAGS_UNIQUE, GTAGS_EXTRACTMETHOD
 */
void
gtagsadd(gtop, comline, path, flags)
GTOP	*gtop;
char	*comline;
char	*path;
int	flags;
{
	char	*tagline;
	FILE	*ip;
	STRBUF	*sb = stropen();

	/*
	 * add path index if not yet.
	 */
	pathput(path);
	/*
	 * make command line.
	 */
	makecommand(comline, path, sb);
	/*
	 * Compact format.
	 */
	if (gtop->format & GTAGS_PATHINDEX) {
		char	*pno;

		if ((pno = pathget(path)) == NULL)
			die1("GPATH is corrupted.('%s' not found)", path);
		strputs(sb, "| sed 's!");
		strputs(sb, path);
		strputs(sb, "!");
		strputs(sb, pno);
		strputs(sb, "!'");
	}
	if (gtop->format & GTAGS_COMPACT)
		strputs(sb, "| sort +0 -1 +1n -2");
	if (flags & GTAGS_UNIQUE)
		strputs(sb, "| uniq");
	if (!(ip = popen(strvalue(sb), "r")))
		die1("cannot execute '%s'.", strvalue(sb));
	while ((tagline = mgets(ip, NULL, MGETS_TAILCUT)) != NULL) {
		char	*tag, *p;

		if (formatcheck(tagline, gtop->format) < 0)
			die1("illegal parser output.\n'%s'", tagline);
		tag = strmake(tagline, " \t");		 /* tag = $1 */
		/*
		 * extract method when class method definition.
		 *
		 * Ex: Class::method(...)
		 *
		 * key	= 'method'
		 * data = 'Class::method  103 ./class.cpp ...'
		 */
		if (flags & GTAGS_EXTRACTMETHOD) {
			if ((p = locatestring(tag, ".", MATCH_LAST)) != NULL)
				tag = p + 1;
			else if ((p = locatestring(tag, "::", MATCH_LAST)) != NULL)
				tag = p + 2;
		}
		gtagsput(gtop, tag, tagline);
	}
	pclose(ip);
	strclose(sb);
}
/*
 * belongto: wheather or not record belongs to the path.
 *
 *	i)	gtop	GTOP structure
 *	i)	path	path name (in standard format)
 *			path number (in compact format)
 *	i)	p	record
 *	r)		1: belong, 0: not belong
 */
static int
belongto(gtop, path, p)
GTOP	*gtop;
char	*path;
char	*p;
{
	char	*q;
	int	length = strlen(path);

	/*
	 * seek to path part.
	 */
	if (gtop->format & GTAGS_PATHINDEX) {
		for (q = p; *q && !isspace(*q); q++)
			;
		if (*q == 0)
			die1("illegal tag format. '%s'", p);
		for (; *q && isspace(*q); q++)
			;
	} else
		q = locatestring(p, "./", MATCH_FIRST);
	if (*q == 0)
		die1("illegal tag format. '%s'", p);
	if (!strncmp(q, path, length) && isspace(*(q + length)))
		return 1;
	return 0;
}
/*
 * gtagsdelete: delete records belong to path.
 *
 *	i)	gtop	GTOP structure
 *	i)	path	path name
 */
void
gtagsdelete(gtop, path)
GTOP	*gtop;
char	*path;
{
	char	*p, *key;
	int	length;

	/*
	 * In compact format, a path is saved as a file number.
	 */
	key = path;
	if (gtop->format & GTAGS_PATHINDEX)
		if ((key = pathget(path)) == NULL)
			die1("GPATH is corrupted.('%s' not found)", path);
	length = strlen(key);
	/*
	 * read sequentially, because db(1) has just one index.
	 */
	for (p = dbop_first(gtop->dbop, NULL, 0); p; p = dbop_next(gtop->dbop))
		if (belongto(gtop, key, p))
			dbop_del(gtop->dbop, NULL);
	/*
	 * don't delete from path index.
	 */
}
/*
 * gtagsfirst: return first record
 *
 *	i)	gtop	GTOP structure
 *	i)	tag	tag name
 *	i)	flags	GTOP_PREFIX	prefix read
 *			GTOP_KEY	read key only
 *	r)		record
 */
char *
gtagsfirst(gtop, tag, flags)
GTOP	*gtop;
char	*tag;
int	flags;
{
	int	dbflags = 0;
	char	*line;

	gtop->flags = flags;
	if (flags & GTOP_PREFIX && tag != NULL)
		dbflags |= DBOP_PREFIX;
	if (flags & GTOP_KEY)
		dbflags |= DBOP_KEY;
	if ((line = dbop_first(gtop->dbop, tag, dbflags)) == NULL)
		return NULL;
	if (gtop->format == GTAGS_STANDARD || gtop->flags & GTOP_KEY)
		return line;
	/*
	 * Compact format.
	 */
	gtop->line = line;			/* gtop->line = $0 */
	gtop->opened = 0;
	return genrecord(gtop);
}
/*
 * gtagsnext: return followed record
 *
 *	i)	gtop	GTOP structure
 *	r)		record
 *			NULL end of tag
 */
char *
gtagsnext(gtop)
GTOP	*gtop;
{
	char	*line;

	/*
	 * If it is standard format or only key.
	 * Just return it.
	 */
	if (gtop->format == GTAGS_STANDARD || gtop->flags & GTOP_KEY)
		return dbop_next(gtop->dbop);
	/*
	 * gtop->format & GTAGS_COMPACT
	 */
	if ((line = genrecord(gtop)) != NULL)
		return line;
	/*
	 * read next record.
	 */
	if ((line = dbop_next(gtop->dbop)) == NULL)
		return line;
	gtop->line = line;			/* gtop->line = $0 */
	gtop->opened = 0;
	return genrecord(gtop);
}
/*
 * gtagsclose: close tag file
 *
 *	i)	gtop	GTOP structure
 */
void
gtagsclose(gtop)
GTOP	*gtop;
{
	if (gtop->format & GTAGS_PATHINDEX || gtop->mode != GTAGS_READ)
		pathclose();
	if (gtop->sb && gtop->prev_tag[0])
		dbop_put(gtop->dbop, gtop->prev_tag, strvalue(gtop->sb));
	if (gtop->sb)
		strclose(gtop->sb);
	dbop_close(gtop->dbop);
	free(gtop);
}
static char *
genrecord(gtop)
GTOP	*gtop;
{
	static char	output[MAXBUFLEN+1];
	char	path[MAXPATHLEN+1];
	static char	buf[1];
	char	*buffer = buf;
	char	*lnop;
	int	tagline;

	if (!gtop->opened) {
		char	*p, *q;

		gtop->opened = 1;
		p = gtop->line;
		q = gtop->tag;				/* gtop->tag = $1 */
		while (!isspace(*p))
			*q++ = *p++;
		*q = 0;
		for (; isspace(*p) ; p++)
			;
		if (gtop->format & GTAGS_PATHINDEX) {	/* gtop->path = $2 */
			char	*name;

			q = path;
			while (!isspace(*p))
				*q++ = *p++;
			*q = 0;
			if ((name = pathget(path)) == NULL)
				die1("GPATH is corrupted.('%s' not found)", path);
			strcpy(gtop->path, name);
		} else {
			q = gtop->path;
			while (!isspace(*p))
				*q++ = *p++;
			*q = 0;
		}
		for (; isspace(*p) ; p++)
			;
		gtop->lnop = p;			/* gtop->lnop = $3 */

		if (gtop->root)
			sprintf(path, "%s/%s", gtop->root, &gtop->path[2]);
		else
			sprintf(path, "%s", &gtop->path[2]);
		if ((gtop->fp = fopen(path, "r")) != NULL) {
			buffer = mgets(gtop->fp, NULL, MGETS_TAILCUT);
			gtop->lno = 1;
		}
	}

	lnop = gtop->lnop;
	if (*lnop >= '0' && *lnop <= '9') {
		/* get line number */
		for (tagline = 0; *lnop >= '0' && *lnop <= '9'; lnop++)
			tagline = tagline * 10 + *lnop - '0';
		if (*lnop == ',')
			lnop++;
		gtop->lnop = lnop;
		if (gtop->fp) {
			if (gtop->lno == tagline)
				return output;
			while (gtop->lno < tagline) {
				if (!(buffer = mgets(gtop->fp, NULL, MGETS_TAILCUT)))
					die1("unexpected end of file. '%s'", path);
				gtop->lno++;
			}
		}
		if (strlen(gtop->tag) >= 16 && tagline >= 1000)
			sprintf(output, "%-16s %4d %-16s %s",
					gtop->tag, tagline, gtop->path, buffer);
		else
			sprintf(output, "%-16s%4d %-16s %s",
					gtop->tag, tagline, gtop->path, buffer);
		return output;
	}
	if (gtop->opened && gtop->fp != NULL) {
		gtop->opened = 0;
		fclose(gtop->fp);
	}
	return NULL;
}
