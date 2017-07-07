/* lib/rpc/getrpcent.c */
/*
 * Copyright (c) 2010, Oracle America, Inc.
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of the "Oracle America, Inc." nor the names of
 *       its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <sys/types.h>
#include <netdb.h>
#include <gssrpc/rpc.h>
#include <string.h>
#include <sys/socket.h>

SETRPCENT_TYPE setrpcent (int);
ENDRPCENT_TYPE endrpcent (void);

/*
 * Internet version.
 */
struct rpcdata {
	FILE	*rpcf;
	char	*current;
	int	currentlen;
	int	stayopen;
#define	MAXALIASES	35
	char	*rpc_aliases[MAXALIASES];
	struct	rpcent rpc;
	char	line[BUFSIZ+1];
	char	*domain;
} *rpcdata;
static struct rpcdata *get_rpcdata();

static	struct rpcent *interpret();
struct	hostent *gethostent();

static char RPCDB[] = "/etc/rpc";

static struct rpcdata *
get_rpcdata(void)
{
	register struct rpcdata *d = rpcdata;

	if (d == 0) {
		d = (struct rpcdata *)calloc(1, sizeof (struct rpcdata));
		rpcdata = d;
	}
	return (d);
}

struct rpcent *
getrpcbynumber(register int number)
{
	register struct rpcdata *d = get_rpcdata();
	register struct rpcent *p;
	int reason;
	char adrstr[16], *val = NULL;
	int vallen;

	if (d == 0)
		return (0);
	setrpcent(0);
	while (p = getrpcent()) {
		if (p->r_number == number)
			break;
	}
	endrpcent();
	return (p);
}

struct rpcent *
getrpcbyname(const char *name)
{
	struct rpcent *rpc;
	char **rp;

	setrpcent(0);
	while(rpc = getrpcent()) {
		if (strcmp(rpc->r_name, name) == 0)
			return (rpc);
		for (rp = rpc->r_aliases; *rp != NULL; rp++) {
			if (strcmp(*rp, name) == 0)
				return (rpc);
		}
	}
	endrpcent();
	return (NULL);
}

SETRPCENT_TYPE setrpcent(int f)
{
	register struct rpcdata *d = _rpcdata();

	if (d == 0)
		return;
	if (d->rpcf == NULL) {
		d->rpcf = fopen(RPCDB, "r");
		if (d->rpcf)
		    set_cloexec_file(d->rpcf);
	} else
		rewind(d->rpcf);
	if (d->current)
		free(d->current);
	d->current = NULL;
	d->stayopen |= f;
}

ENDRPCENT_TYPE endrpcent(void)
{
	register struct rpcdata *d = _rpcdata();

	if (d == 0)
		return;
	if (d->current && !d->stayopen) {
		free(d->current);
		d->current = NULL;
	}
	if (d->rpcf && !d->stayopen) {
		fclose(d->rpcf);
		d->rpcf = NULL;
	}
}

struct rpcent *
getrpcent(void)
{
	struct rpcent *hp;
	int reason;
	char *key = NULL, *val = NULL;
	int keylen, vallen;
	register struct rpcdata *d = _rpcdata();

	if (d == 0)
		return(NULL);
	if (d->rpcf == NULL) {
	    if ((d->rpcf = fopen(RPCDB, "r")) == NULL)
		return (NULL);
	    set_cloexec_file(d->rpcf);
	}
	if (fgets(d->line, BUFSIZ, d->rpcf) == NULL)
		return (NULL);
	return interpret(d->line, strlen(d->line));
}

static struct rpcent *
interpret(char *val, int len)
{
	register struct rpcdata *d = _rpcdata();
	char *p;
	register char *cp, **q;

	if (d == 0)
		return;
	strncpy(d->line, val, len);
	p = d->line;
	d->line[len] = '\n';
	if (*p == '#')
		return (getrpcent());
	cp = strchr(p, '#');
	if (cp == NULL)
    {
		cp = strchr(p, '\n');
		if (cp == NULL)
			return (getrpcent());
	}
	*cp = '\0';
	cp = strchr(p, ' ');
	if (cp == NULL)
    {
		cp = strchr(p, '\t');
		if (cp == NULL)
			return (getrpcent());
	}
	*cp++ = '\0';
	/* THIS STUFF IS INTERNET SPECIFIC */
	d->rpc.r_name = d->line;
	while (*cp == ' ' || *cp == '\t')
		cp++;
	d->rpc.r_number = atoi(cp);
	q = d->rpc.r_aliases = d->rpc_aliases;
	cp = strchr(p, ' ');
	if (cp != NULL)
		*cp++ = '\0';
	else
    {
		cp = strchr(p, '\t');
		if (cp != NULL)
			*cp++ = '\0';
	}
	while (cp && *cp) {
		if (*cp == ' ' || *cp == '\t') {
			cp++;
			continue;
		}
		if (q < &(d->rpc_aliases[MAXALIASES - 1]))
			*q++ = cp;
		cp = strchr(p, ' ');
		if (cp != NULL)
			*cp++ = '\0';
		else
	    {
			cp = strchr(p, '\t');
			if (cp != NULL)
				*cp++ = '\0';
		}
	}
	*q = NULL;
	return (&d->rpc);
}
