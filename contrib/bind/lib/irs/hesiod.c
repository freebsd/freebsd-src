#if defined(LIBC_SCCS) && !defined(lint)
static const char rcsid[] = "$Id: hesiod.c,v 1.23 2002/07/18 02:07:45 marka Exp $";
#endif

/*
 * Copyright (c) 1996,1999 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

/*
 * This file is primarily maintained by <tytso@mit.edu> and <ghudson@mit.edu>.
 */

/*
 * hesiod.c --- the core portion of the hesiod resolver.
 *
 * This file is derived from the hesiod library from Project Athena;
 * It has been extensively rewritten by Theodore Ts'o to have a more
 * thread-safe interface.
 */

/* Imports */

#include "port_before.h"

#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/nameser.h>

#include <errno.h>
#include <netdb.h>
#include <resolv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "port_after.h"

#include "pathnames.h"
#include "hesiod.h"
#include "hesiod_p.h"

/* Forward */

int		hesiod_init(void **context);
void		hesiod_end(void *context);
char *		hesiod_to_bind(void *context, const char *name,
			       const char *type);
char **		hesiod_resolve(void *context, const char *name,
			       const char *type);
void		hesiod_free_list(void *context, char **list);

static int	parse_config_file(struct hesiod_p *ctx, const char *filename);
static char **	get_txt_records(struct hesiod_p *ctx, int class,
				const char *name);
static int	init(struct hesiod_p *ctx);

/* Public */

/*
 * This function is called to initialize a hesiod_p.
 */
int
hesiod_init(void **context) {
	struct hesiod_p *ctx;
	char *cp;

	ctx = malloc(sizeof(struct hesiod_p));
	if (ctx == 0) {
		errno = ENOMEM;
		return (-1);
	}

	ctx->LHS = NULL;
	ctx->RHS = NULL;
	ctx->res = NULL;

	if (parse_config_file(ctx, _PATH_HESIOD_CONF) < 0) {
#ifdef DEF_RHS
		/*
		 * Use compiled in defaults.
		 */
		ctx->LHS = malloc(strlen(DEF_LHS)+1);
		ctx->RHS = malloc(strlen(DEF_RHS)+1);
		if (ctx->LHS == 0 || ctx->RHS == 0) {
			errno = ENOMEM;
			goto cleanup;
		}
#ifdef HAVE_STRLCPY
		strlcpy(ctx->LHS, DEF_LHS, strlen(DEF_LHS) + 1);
		strlcpy(ctx->RHS, DEF_RHS, strlen(DEF_RHS) + 1);
#else
		strcpy(ctx->LHS, DEF_LHS);
		strcpy(ctx->RHS, DEF_RHS);
#endif
#else
		goto cleanup;
#endif
	}
	/*
	 * The default RHS can be overridden by an environment
	 * variable.
	 */
	if ((cp = getenv("HES_DOMAIN")) != NULL) {
		size_t RHSlen = strlen(cp) + 2;
		if (ctx->RHS)
			free(ctx->RHS);
		ctx->RHS = malloc(RHSlen);
		if (!ctx->RHS) {
			errno = ENOMEM;
			goto cleanup;
		}
		if (cp[0] == '.') {
#ifdef HAVE_STRLCPY
			strlcpy(ctx->RHS, cp, RHSlen);
#else
			strcpy(ctx->RHS, cp);
#endif
		} else {
#ifdef HAVE_STRLCPY
			strlcpy(ctx->RHS, ".", RHSlen);
#else
			strcpy(ctx->RHS, ".");
#endif
#ifdef HAVE_STRLCAT
			strlcat(ctx->RHS, cp, RHSlen);
#else
			strcat(ctx->RHS, cp);
#endif
		}
	}

	/*
	 * If there is no default hesiod realm set, we return an
	 * error.
	 */
	if (!ctx->RHS) {
		errno = ENOEXEC;
		goto cleanup;
	}
	
#if 0
	if (res_ninit(ctx->res) < 0)
		goto cleanup;
#endif

	*context = ctx;
	return (0);

 cleanup:
	hesiod_end(ctx);
	return (-1);
}

/*
 * This function deallocates the hesiod_p
 */
void
hesiod_end(void *context) {
	struct hesiod_p *ctx = (struct hesiod_p *) context;
	int save_errno = errno;

	if (ctx->res)
		res_nclose(ctx->res);
	if (ctx->RHS)
		free(ctx->RHS);
	if (ctx->LHS)
		free(ctx->LHS);
	if (ctx->res && ctx->free_res)
		(*ctx->free_res)(ctx->res);
	free(ctx);
	errno = save_errno;
}

/*
 * This function takes a hesiod (name, type) and returns a DNS
 * name which is to be resolved.
 */
char *
hesiod_to_bind(void *context, const char *name, const char *type) {
	struct hesiod_p *ctx = (struct hesiod_p *) context;
	char *bindname;
	char **rhs_list = NULL;
	const char *RHS, *cp;

	/* Decide what our RHS is, and set cp to the end of the actual name. */
	if ((cp = strchr(name, '@')) != NULL) {
		if (strchr(cp + 1, '.'))
			RHS = cp + 1;
		else if ((rhs_list = hesiod_resolve(context, cp + 1,
		    "rhs-extension")) != NULL)
			RHS = *rhs_list;
		else {
			errno = ENOENT;
			return (NULL);
		}
	} else {
		RHS = ctx->RHS;
		cp = name + strlen(name);
	}

	/*
	 * Allocate the space we need, including up to three periods and
	 * the terminating NUL.
	 */
	if ((bindname = malloc((cp - name) + strlen(type) + strlen(RHS) +
	    (ctx->LHS ? strlen(ctx->LHS) : 0) + 4)) == NULL) {
		errno = ENOMEM;
		if (rhs_list)
			hesiod_free_list(context, rhs_list);
		return NULL;
	}

	/* Now put together the DNS name. */
	memcpy(bindname, name, cp - name);
	bindname[cp - name] = '\0';
	strcat(bindname, ".");
	strcat(bindname, type);
	if (ctx->LHS) {
		if (ctx->LHS[0] != '.')
			strcat(bindname, ".");
		strcat(bindname, ctx->LHS);
	}
	if (RHS[0] != '.')
		strcat(bindname, ".");
	strcat(bindname, RHS);

	if (rhs_list)
		hesiod_free_list(context, rhs_list);

	return (bindname);
}

/*
 * This is the core function.  Given a hesiod (name, type), it
 * returns an array of strings returned by the resolver.
 */
char **
hesiod_resolve(void *context, const char *name, const char *type) {
	struct hesiod_p *ctx = (struct hesiod_p *) context;
	char *bindname = hesiod_to_bind(context, name, type);
	char **retvec;

	if (bindname == NULL)
		return (NULL);
	if (init(ctx) == -1) {
		free(bindname);
		return (NULL);
	}

	if ((retvec = get_txt_records(ctx, C_IN, bindname))) {
		free(bindname);
		return (retvec);
	}
	
	if (errno != ENOENT)
		return (NULL);

	retvec = get_txt_records(ctx, C_HS, bindname);
	free(bindname);
	return (retvec);
}

void
hesiod_free_list(void *context, char **list) {
	char **p;

	UNUSED(context);

	for (p = list; *p; p++)
		free(*p);
	free(list);
}

/*
 * This function parses the /etc/hesiod.conf file
 */
static int
parse_config_file(struct hesiod_p *ctx, const char *filename) {
	char *key, *data, *cp, **cpp;
	char buf[MAXDNAME+7];
	FILE *fp;

	/*
	 * Clear the existing configuration variable, just in case
	 * they're set.
	 */
	if (ctx->RHS)
		free(ctx->RHS);
	if (ctx->LHS)
		free(ctx->LHS);
	ctx->RHS = ctx->LHS = 0;

	/*
	 * Now open and parse the file...
	 */
	if (!(fp = fopen(filename, "r")))
		return (-1);
	
	while (fgets(buf, sizeof(buf), fp) != NULL) {
		cp = buf;
		if (*cp == '#' || *cp == '\n' || *cp == '\r')
			continue;
		while(*cp == ' ' || *cp == '\t')
			cp++;
		key = cp;
		while(*cp != ' ' && *cp != '\t' && *cp != '=')
			cp++;
		*cp++ = '\0';
		
		while(*cp == ' ' || *cp == '\t' || *cp == '=')
			cp++;
		data = cp;
		while(*cp != ' ' && *cp != '\n' && *cp != '\r')
			cp++;
		*cp++ = '\0';

		if (strcmp(key, "lhs") == 0)
			cpp = &ctx->LHS;
		else if (strcmp(key, "rhs") == 0)
			cpp = &ctx->RHS;
		else
			continue;

		*cpp = malloc(strlen(data) + 1);
		if (!*cpp) {
			errno = ENOMEM;
			goto cleanup;
		}
		strcpy(*cpp, data);
	}
	fclose(fp);
	return (0);
	
 cleanup:
	fclose(fp);
	if (ctx->RHS)
		free(ctx->RHS);
	if (ctx->LHS)
		free(ctx->LHS);
	ctx->RHS = ctx->LHS = 0;
	return (-1);
}

/*
 * Given a DNS class and a DNS name, do a lookup for TXT records, and
 * return a list of them.
 */
static char **
get_txt_records(struct hesiod_p *ctx, int class, const char *name) {
	struct {
		int type;		/* RR type */
		int class;		/* RR class */
		int dlen;		/* len of data section */
		u_char *data;		/* pointer to data */
	} rr;
	HEADER *hp;
	u_char qbuf[MAX_HESRESP], abuf[MAX_HESRESP];
	u_char *cp, *erdata, *eom;
	char *dst, *edst, **list;
	int ancount, qdcount;
	int i, j, n, skip;

	/*
	 * Construct the query and send it.
	 */
	n = res_nmkquery(ctx->res, QUERY, name, class, T_TXT, NULL, 0,
			 NULL, qbuf, MAX_HESRESP);
	if (n < 0) {
		errno = EMSGSIZE;
		return (NULL);
	}
	n = res_nsend(ctx->res, qbuf, n, abuf, MAX_HESRESP);
	if (n < 0) {
		errno = ECONNREFUSED;
		return (NULL);
	}
	if (n < HFIXEDSZ) {
		errno = EMSGSIZE;
		return (NULL);
	}

	/*
	 * OK, parse the result.
	 */
	hp = (HEADER *) abuf;
	ancount = ntohs(hp->ancount);
	qdcount = ntohs(hp->qdcount);
	cp = abuf + sizeof(HEADER);
	eom = abuf + n;

	/* Skip query, trying to get to the answer section which follows. */
	for (i = 0; i < qdcount; i++) {
		skip = dn_skipname(cp, eom);
		if (skip < 0 || cp + skip + QFIXEDSZ > eom) {
			errno = EMSGSIZE;
			return (NULL);
		}
		cp += skip + QFIXEDSZ;
	}

	list = malloc((ancount + 1) * sizeof(char *));
	if (!list) {
		errno = ENOMEM;
		return (NULL);
	}
	j = 0;
	for (i = 0; i < ancount; i++) {
		skip = dn_skipname(cp, eom);
		if (skip < 0) {
			errno = EMSGSIZE;
			goto cleanup;
		}
		cp += skip;
		if (cp + 3 * INT16SZ + INT32SZ > eom) {
			errno = EMSGSIZE;
			goto cleanup;
		}
		rr.type = ns_get16(cp);
		cp += INT16SZ;
		rr.class = ns_get16(cp);
		cp += INT16SZ + INT32SZ;	/* skip the ttl, too */
		rr.dlen = ns_get16(cp);
		cp += INT16SZ;
		if (cp + rr.dlen > eom) {
			errno = EMSGSIZE;
			goto cleanup;
		}
		rr.data = cp;
		cp += rr.dlen;
		if (rr.class != class || rr.type != T_TXT)
			continue;
		if (!(list[j] = malloc(rr.dlen)))
			goto cleanup;
		dst = list[j++];
		edst = dst + rr.dlen;
		erdata = rr.data + rr.dlen;
		cp = rr.data;
		while (cp < erdata) {
			n = (unsigned char) *cp++;
			if (cp + n > eom || dst + n > edst) {
				errno = EMSGSIZE;
				goto cleanup;
			}
			memcpy(dst, cp, n);
			cp += n;
			dst += n;
		}
		if (cp != erdata) {
			errno = EMSGSIZE;
			goto cleanup;
		}
		*dst = '\0';
	}
	list[j] = NULL;
	if (j == 0) {
		errno = ENOENT;
		goto cleanup;
	}
	return (list);

 cleanup:
	for (i = 0; i < j; i++)
		free(list[i]);
	free(list);
	return (NULL);
}

struct __res_state *
__hesiod_res_get(void *context) {
	struct hesiod_p *ctx = context;

	if (!ctx->res) {
		struct __res_state *res;
		res = (struct __res_state *)malloc(sizeof *res);
		if (res == NULL) {
			errno = ENOMEM;
			return (NULL);
		}
		memset(res, 0, sizeof *res);
		__hesiod_res_set(ctx, res, free);
	}

	return (ctx->res);
}

void
__hesiod_res_set(void *context, struct __res_state *res,
	         void (*free_res)(void *)) {
	struct hesiod_p *ctx = context;

	if (ctx->res && ctx->free_res) {
		res_nclose(ctx->res);
		(*ctx->free_res)(ctx->res);
	}

	ctx->res = res;
	ctx->free_res = free_res;
}

static int
init(struct hesiod_p *ctx) {
	
	if (!ctx->res && !__hesiod_res_get(ctx))
		return (-1);

	if (((ctx->res->options & RES_INIT) == 0) &&
	    (res_ninit(ctx->res) == -1))
		return (-1);

	return (0);
}
