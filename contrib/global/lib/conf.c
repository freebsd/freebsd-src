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
 *	conf.c					30-Jun-98
 *
 */
#include <sys/param.h>
#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "conf.h"
#include "die.h"
#include "gparam.h"
#include "locatestring.h"
#include "makepath.h"
#include "mgets.h"
#include "strbuf.h"
#include "strmake.h"
#include "test.h"
/*
 * Access library for gtags.conf (.gtagsrc).
 * File format is a subset of XXXcap (termcap, printcap) file.
 */
#define	GTAGSCONF	"/etc/global.conf"
#define GTAGSRC		".globalrc"
#define DEFAULTLABEL	"default"
static FILE	*fp;
static char	*line;
static int	allowed_nest_level = 8;
static int	opened;

static void	trim __P((char *));
static char	*readrecord __P((const char *));
static void	includelabel __P((STRBUF *, const char *, int));

static void
trim(l)
char	*l;
{
	char	*f, *b;
	int	colon = 0;

	for (f = b = l; *f; f++) {
		if (colon && isspace(*f))
			continue;
		colon = 0;
		if ((*b++ = *f) == ':')
			colon = 1;
	}
	*b = 0;
}
static char	*
readrecord(label)
const char *label;
{
	char	*p, *q;

	rewind(fp);
	while ((p = mgets(fp, NULL, MGETS_CONT|MGETS_SKIPCOM)) != NULL) {
		trim(p);
		for (;;) {
			if ((q = strmake(p, "|:")) == NULL)
				die1("illegal configuration file format (%s).", p);
			if (!strcmp(label, q)) {
				if (!(p = locatestring(p, ":", MATCH_FIRST)))
					die("illegal configuration file format.");
				p = strdup(p);
				if (!p)
					die("short of memory.");
				return p;
			}
			p += strlen(q);
			if (*p == ':')
				break;
			else if (*p == '|')
				p++;
			else
				assert(0);
		}
	}
	return NULL;
}
static	void
includelabel(sb, label, level)
STRBUF	*sb;
const char *label;
int	level;
{
	char	*savep, *p, *q;

	if (++level > allowed_nest_level)
		die("nested include= (or tc=) over flow.");
	if (!(savep = p = readrecord(label)))
		die1("label '%s' not found.", label);
	while ((q = locatestring(p, ":include=", MATCH_FIRST)) || (q = locatestring(p, ":tc=", MATCH_FIRST))) {
		char	inclabel[MAXPROPLEN+1], *c = inclabel;

		strnputs(sb, p, q - p);
		q = locatestring(q, "=", MATCH_FIRST) + 1;
		while (*q && *q != ':')
			*c++ = *q++;
		*c = 0;
		includelabel(sb, inclabel, level);
		p = q;
	}
	strputs(sb, p);
	free(savep);
}
/*
 * configpath: get path of configuration file.
 */
char *
configpath() {
	static char config[MAXPATHLEN+1];
	char *p;

	if ((p = getenv("GTAGSCONF")) != NULL) {
		if (!test("r", p))
			config[0] = 0;
		else
			strcpy(config, p);
	} else if ((p = getenv("HOME")) && test("r", makepath(p, GTAGSRC)))
		strcpy(config, makepath(p, GTAGSRC));
	else if (test("r", GTAGSCONF))
		strcpy(config, GTAGSCONF);
	else
		config[0] = 0;
	return config;
}
/*
 * openconf: load configuration file.
 *
 *	go)	line	specified entry
 */
void
openconf()
{
	const char *label, *config;
	STRBUF	*sb;

	assert(opened == 0);

	config = configpath();
	/*
	 * if configuration file is not found, default values are set
	 * for upper compatibility.
	 */
	if (*config == 0) {
		sb = stropen();
		strputs(sb, "suffixes=c,h,y,s,S,java:");
		strputs(sb, "skip=y.tab.c,y.tab.h,SCCS/,RCS/,CVS/:");
		strputs(sb, "format=standard:");
		strputs(sb, "extractmethod:");
		strputs(sb, "GTAGS=gctags %s:");
		strputs(sb, "GRTAGS=gctags -r %s:");
		strputs(sb, "GSYMS=gctags -s %s:");
		line = strdup(strvalue(sb));
		if (!line)
			die("short of memory.");
		strclose(sb);
		opened = 1;
		return;
	}
	if ((label = getenv("GTAGSLABEL")) == NULL)
		label = "default";
	if (!(fp = fopen(config, "r")))
		die1("cannot open '%s'.", config);
	sb = stropen();
	includelabel(sb, label, 0);
	line = strdup(strvalue(sb));
	strclose(sb);
	fclose(fp);
	opened = 1;
	return;
}
/*
 * getconfn: get property number
 *
 *	i)	name	property name
 *	o)	num	value (if not NULL)
 *	r)		1: found, 0: not found
 */
int
getconfn(name, num)
const char *name;
int	*num;
{
	char	*p;
	char	buf[MAXPROPLEN+1];

	if (!opened)
		openconf();
	sprintf(buf, ":%s#", name);
	if ((p = locatestring(line, buf, MATCH_FIRST)) != NULL) {
		p += strlen(buf);
		if (num != NULL)
			*num = atoi(p);
		return 1;
	}
	return 0;
}
/*
 * getconfs: get property string
 *
 *	i)	name	property name
 *	o)	sb	string buffer (if not NULL)
 *	r)		1: found, 0: not found
 */
int
getconfs(name, sb)
const char *name;
STRBUF	*sb;
{
	char	*p;
	char	buf[MAXPROPLEN+1];
	int	all = 0;
	int	exist = 0;

	if (!opened)
		openconf();
	if (!strcmp(name, "suffixes") || !strcmp(name, "skip") || !strcmp(name, "reserved_words"))
		all = 1;
	sprintf(buf, ":%s=", name);
	p = line;
	while ((p = locatestring(p, buf, MATCH_FIRST)) != NULL) {
		if (exist && sb)
			strputc(sb, ',');		
		exist = 1;
		for (p += strlen(buf); *p && *p != ':'; p++)
			if (sb)
				strputc(sb, *p);
		if (!all)
			break;
	}
	/*
	 * It may be that these code should be moved to applications.
	 * But nothing cannot start without them.
	 */
	if (!exist) {
		exist = 1;
		if (!strcmp(name, "suffixes")) {
			if (sb)
				strputs(sb, "c,h,y,s,S,java");
		} else if (!strcmp(name, "skip")) {
			if (sb)
				strputs(sb, "y.tab.c,y.tab.h,SCCS/,RCS/,CVS/");
		} else
			exist = 0;
	}
	return exist;
}
/*
 * getconfb: get property bool value
 *
 *	i)	name	property name
 *	r)		1: TRUE, 0: FALSE
 */
int
getconfb(name)
const char *name;
{
	char	*p;
	char	buf[MAXPROPLEN+1];

	if (!opened)
		openconf();
	sprintf(buf, ":%s:", name);
	if ((p = locatestring(line, buf, MATCH_FIRST)) != NULL)
		return 1;
	return 0;
}
void
closeconf()
{
	if (!opened)
		return;
	free(line);
	opened = 0;
}
