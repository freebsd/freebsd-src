/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $FreeBSD$
 *
 * Copyright (c) 2001
 *      Andrey A. Chernov.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    verbatim and that no modifications are made prior to this
 *    point in the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY ANDREY A. CHERNOV ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL ANDREY A. CHERNOV OR HIS PETS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, LIFE OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include "sysinstall.h"
#include <sys/stat.h>
#include <ctype.h>
#include <ttyent.h>

#define _X_EXTENSION ".XXXXXX"

void
configTtys(void)
{
    size_t len;
    int t, tlen, changed;
    FILE *fp, *np;
    char sq, *line, *p, *q, *cp, *tptr;
    char templ[sizeof(_PATH_TTYS) + sizeof(_X_EXTENSION) - 1];
    struct ttyent *tnam;
    
    if ((cp = variable_get(VAR_CONSTERM)) == NULL ||
	strcmp(cp, "NO") == 0)
	return;
    if (!file_readable(_PATH_TTYS)) {
	msgConfirm("%s not exist or not readable", _PATH_TTYS);
	return;
    }
    if ((fp = fopen(_PATH_TTYS, "r")) == NULL) {
	msgConfirm("Can't open %s for read: %s", _PATH_TTYS,
		   strerror(errno));
	return;
    }
    strcpy(templ, _PATH_TTYS _X_EXTENSION);
    if ((t = mkstemp(templ)) < 0) {
	msgConfirm("Can't create %s: %s", templ, strerror(errno));
	(void)fclose(fp);
	return;
    }
    if (fchmod(t, 0644)) {
	msgConfirm("Can't fchmod %s: %s", templ, strerror(errno));
	(void)fclose(fp);
	return;
    }
    if ((np = fdopen(t, "w")) == NULL) {
	msgConfirm("Can't fdopen %s: %s", templ, strerror(errno));
	(void)close(t);
	(void)fclose(fp);
	(void)unlink(templ);
	return;
    }
    changed = 0;
    while ((line = fgetln(fp, &len)) != NULL) {
	p = line;
	while (p < (line + len) && isspace((unsigned char)*p))
	    ++p;
	if (strncmp(p, "ttyv", 4) != 0) {
    dump:
	    if (fwrite(line, len, 1, np) != 1) {
    wrerr:
		msgConfirm("%s: write error: %s", templ, strerror(errno));
		(void)fclose(fp);
		(void)fclose(np);
		(void)unlink(templ);
		return;
	    }
	} else {
	    q = p;
	    while(q < (line + len) && !isspace((unsigned char)*q))
		++q;
	    if (!isspace((unsigned char)*q))
		goto dump;
	    sq = *q;
	    *q = '\0';
	    tnam = getttynam(p);
	    *q = sq;
	    if (tnam == NULL || tnam->ty_type == NULL ||
		strcmp(tnam->ty_type, cp) == 0 ||
		strncmp(tnam->ty_type, "cons", 4) != 0 ||
		!isdigit((unsigned char)tnam->ty_type[4])
	       )
		goto dump;
	    tlen = strlen(tnam->ty_type);
	    tptr = NULL;
	    p = ++q;
	    while(p < (line + len)) {
		if (strncmp(p, tnam->ty_type, tlen) == 0) {
		    tptr = p;
		    break;
		}
		++p;
	    }
	    if (tptr == NULL)
		goto dump;
	    changed = 1;
	    if (fwrite(line, tptr - line, 1, np) != 1 ||
		fputs(cp, np) ||
		fwrite(tptr + tlen,
		       len - (tptr + tlen - line), 1, np) != 1)
		goto wrerr;
	}
    }
    if (!feof(fp)) {
	msgConfirm("%s: read error: %s", _PATH_TTYS, strerror(errno));
	(void)fclose(fp);
	(void)fclose(np);
	(void)unlink(templ);
	return;
    }
    (void)fclose(fp);
    if (fclose(np)) {
	if (changed)
		msgConfirm("%s: close error: %s", templ, strerror(errno));
	else
		variable_set2(VAR_CONSTERM, "NO", 0);
	(void)unlink(templ);
	return;
    }
    if (!changed) {
	(void)unlink(templ);
	variable_set2(VAR_CONSTERM, "NO", 0);
	return;
    }
    if (rename(templ, _PATH_TTYS)) {
	msgConfirm("Can't rename %s to %s: %s", templ, _PATH_TTYS,
		   strerror(errno));
	return;
    }
    variable_set2(VAR_CONSTERM, "NO", 0);
}
