/*
 * Copyright (C) 2006 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * $Id: load_file.c,v 1.1.2.1 2006/08/25 21:13:04 darrenr Exp $
 */

#include "ipf.h"

alist_t *
load_file(char *filename)
{
	alist_t *a, *rtop, *rbot;
	char *s, line[1024], *t;
	int linenum, not;
	FILE *fp;

	fp = fopen(filename + 7, "r");
	if (fp == NULL) {
		fprintf(stderr, "load_file cannot open '%s'\n", filename);
		return NULL;
	}       

	a = NULL;
	rtop = NULL;
	rbot = NULL;
	linenum = 0;    
		
	while (fgets(line, sizeof(line) - 1, fp)) {
		line[sizeof(line) - 1] = '\0';
		linenum++;
		/*
		 * Hunt for CR/LF.  If no LF, stop processing.
		 */
		s = strchr(line, '\n');
		if (s == NULL) {
			fprintf(stderr, "%d:%s: line too long\n", linenum, filename);
			fclose(fp);
			alist_free(rtop);
			return NULL;
		}

		*s = '\0';
		s = strchr(line, '\r');
		if (s != NULL)
			*s = '\0';
		for (t = line; isspace(*t); t++)
			;
		if (*t == '!') {
			not = 1;
			t++;
		} else
			not = 0;

		/*
		 * Remove comment markers
		 */
		for (s = t; *s; s++) {
			if (*s == '#')
				*s = '\0';
		}
		if (!*t)
			continue;
		/*
		 * Trim off tailing white spaces
		 */
		s = strlen(t) + t - 1;
		while (isspace(*s))
			*s-- = '\0';

		if (isdigit(*t)) {
			a = alist_new(4, t);
			a->al_not = not;
			if (rbot != NULL)
				rbot->al_next = a;
			else
				rtop = a;
			rbot = a;
		} else {
			fprintf(stderr, "%s: unrecognised content line %d\n",
				filename, linenum);
		}
	}
	fclose(fp);

	return rtop;
}
