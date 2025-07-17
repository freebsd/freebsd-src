/*
 * Copyright (c) 1983, 1993	The Regents of the University of California.
 * Copyright (c) 1993 Digital Equipment Corporation.
 * Copyright (c) 2012 G. Vanem <gvanem@yahoo.no>.
 * Copyright (c) 2017 Ali Abdulkadir <autostart.ini@gmail.com>.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <config.h>

#include <netdissect-stdinc.h>
#include <getservent.h>

static FILE *servf = NULL;
static char line[BUFSIZ+1];
static struct servent serv;
static char *serv_aliases[MAXALIASES];
int _serv_stayopen;
const char *etc_path(const char *file);

/*
* Check if <file> exists in the current directory and, if so, return it.
* Else return either "%SYSTEMROOT%\System32\drivers\etc\<file>"
* or $PREFIX/etc/<file>.
* "<file>" is aka __PATH_SERVICES (aka "services" on Windows and
* "/etc/services" on other platforms that would need this).
*/
const char *etc_path(const char *file)
{
    const char *env = getenv(__PATH_SYSROOT);
    static char path[_MAX_PATH];

    /* see if "<file>" exists locally or whether __PATH_SYSROOT is valid */
    if (fopen(file, "r") || !env)
        return (file);
    else
#ifdef _WIN32
    snprintf(path, sizeof(path), "%s%s%s", env, __PATH_ETC_INET, file);
#else
    snprintf(path, sizeof(path), "%s%s", env, file);
#endif
    return (path);
}

void
setservent(int f)
{
    if (servf == NULL)
        servf = fopen(etc_path(__PATH_SERVICES), "r");
    else
        rewind(servf);
    _serv_stayopen |= f;
}

void
endservent(void)
{
    if (servf) {
        fclose(servf);
        servf = NULL;
    }
    _serv_stayopen = 0;
}

struct servent *
getservent(void)
{
    char *p;
    char *cp, **q;

    if (servf == NULL && (servf = fopen(etc_path(__PATH_SERVICES), "r")) == NULL)
        return (NULL);

again:
    if ((p = fgets(line, BUFSIZ, servf)) == NULL)
        return (NULL);
    if (*p == '#')
        goto again;
    cp = strpbrk(p, "#\n");
    if (cp == NULL)
        goto again;
    *cp = '\0';
    serv.s_name = p;
    p = strpbrk(p, " \t");
    if (p == NULL)
        goto again;
    *p++ = '\0';
    while (*p == ' ' || *p == '\t')
        p++;
    cp = strpbrk(p, ",/");
    if (cp == NULL)
        goto again;
    *cp++ = '\0';
    serv.s_port = htons((u_short)atoi(p));
    serv.s_proto = cp;
    q = serv.s_aliases = serv_aliases;
    cp = strpbrk(cp, " \t");
    if (cp != NULL)
        *cp++ = '\0';
    while (cp && *cp) {
        if (*cp == ' ' || *cp == '\t') {
            cp++;
            continue;
        }
        if (q < &serv_aliases[MAXALIASES - 1])
            *q++ = cp;
        cp = strpbrk(cp, " \t");
        if (cp != NULL)
            *cp++ = '\0';
    }
    *q = NULL;
    return (&serv);
}
