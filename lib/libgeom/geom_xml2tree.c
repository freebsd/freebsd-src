/*-
 * Copyright (c) 2003 Poul-Henning Kamp
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
 * 3. The names of the authors may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
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
 * $FreeBSD$
 */

#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/queue.h>
#include <sys/sbuf.h>
#include <sys/sysctl.h>
#include <err.h>
#include <bsdxml.h>
#include <libgeom.h>

struct mystate {
	struct gmesh		*mesh;
	struct gclass		*class;
	struct ggeom		*geom;
	struct gprovider	*provider;
	struct gconsumer	*consumer;
	int			level;
	struct sbuf		*sbuf[20];
	struct gconf		*config;
	int			nident;
};

static void
StartElement(void *userData, const char *name, const char **attr)
{
	struct mystate *mt;
	void *id;
	void *ref;
	int i;

	mt = userData;
	mt->level++;
	mt->sbuf[mt->level] = sbuf_new(NULL, NULL, 0, SBUF_AUTOEXTEND);
	id = NULL;
	for (i = 0; attr[i] != NULL; i += 2) {
		if (!strcmp(attr[i], "id")) {
			id = (void *)strtoul(attr[i + 1], NULL, 0);
			mt->nident++;
		} else if (!strcmp(attr[i], "ref")) {
			ref = (void *)strtoul(attr[i + 1], NULL, 0);
		} else
			printf("%*.*s[%s = %s]\n",
			    mt->level + 1, mt->level + 1, "",
			    attr[i], attr[i + 1]);
	}
	if (!strcmp(name, "class") && mt->class == NULL) {
		mt->class = calloc(1, sizeof *mt->class);
		mt->class->id = id;
		LIST_INSERT_HEAD(&mt->mesh->class, mt->class, class);
		LIST_INIT(&mt->class->geom);
		LIST_INIT(&mt->class->config);
		return;
	}
	if (!strcmp(name, "geom") && mt->geom == NULL) {
		mt->geom = calloc(1, sizeof *mt->geom);
		mt->geom->id = id;
		LIST_INSERT_HEAD(&mt->class->geom, mt->geom, geom);
		LIST_INIT(&mt->geom->provider);
		LIST_INIT(&mt->geom->consumer);
		LIST_INIT(&mt->geom->config);
		return;
	}
	if (!strcmp(name, "class") && mt->geom != NULL) {
		mt->geom->class = ref;
		return;
	}
	if (!strcmp(name, "consumer") && mt->consumer == NULL) {
		mt->consumer = calloc(1, sizeof *mt->consumer);
		mt->consumer->id = id;
		LIST_INSERT_HEAD(&mt->geom->consumer, mt->consumer, consumer);
		LIST_INIT(&mt->consumer->config);
		return;
	}
	if (!strcmp(name, "geom") && mt->consumer != NULL) {
		mt->consumer->geom = ref;
		return;
	}
	if (!strcmp(name, "provider") && mt->consumer != NULL) {
		mt->consumer->provider = ref;
		return;
	}
	if (!strcmp(name, "provider") && mt->provider == NULL) {
		mt->provider = calloc(1, sizeof *mt->provider);
		mt->provider->id = id;
		LIST_INSERT_HEAD(&mt->geom->provider, mt->provider, provider);
		LIST_INIT(&mt->provider->consumers);
		LIST_INIT(&mt->provider->config);
		return;
	}
	if (!strcmp(name, "geom") && mt->provider != NULL) {
		mt->provider->geom = ref;
		return;
	}
	if (!strcmp(name, "config")) {
		if (mt->provider != NULL) {
			mt->config = &mt->provider->config;
			return;
		}
		if (mt->consumer != NULL) {
			mt->config = &mt->consumer->config;
			return;
		}
		if (mt->geom != NULL) {
			mt->config = &mt->geom->config;
			return;
		}
		if (mt->class != NULL) {
			mt->config = &mt->class->config;
			return;
		}
	}
}

static void
EndElement(void *userData, const char *name)
{
	struct mystate *mt;
	struct gconfig *gc;
	char *p;

	mt = userData;
	sbuf_finish(mt->sbuf[mt->level]);
	p = strdup(sbuf_data(mt->sbuf[mt->level]));
	sbuf_delete(mt->sbuf[mt->level]);
	mt->sbuf[mt->level] = NULL;
	mt->level--;
	if (strlen(p) == 0) {
		free(p);
		p = NULL;
	}

	if (!strcmp(name, "name")) {
		if (mt->provider != NULL) {
			mt->provider->name = p;
			return;
		} else if (mt->geom != NULL) {
			mt->geom->name = p;
			return;
		} else if (mt->class != NULL) {
			mt->class->name = p;
			return;
		}
	}
	if (!strcmp(name, "rank") && mt->geom != NULL) {
		mt->geom->rank = strtoul(p, NULL, 0);
		free(p);
		return;
	}
	if (!strcmp(name, "mode") && mt->provider != NULL) {
		mt->provider->mode = p;
		return;
	}
	if (!strcmp(name, "mode") && mt->consumer != NULL) {
		mt->consumer->mode = p;
		return;
	}
	if (!strcmp(name, "mediasize") && mt->provider != NULL) {
		mt->provider->mediasize = strtoumax(p, NULL, 0);
		free(p);
		return;
	}
	if (!strcmp(name, "sectorsize") && mt->provider != NULL) {
		mt->provider->sectorsize = strtoul(p, NULL, 0);
		free(p);
		return;
	}

	if (!strcmp(name, "config")) {
		mt->config = NULL;
		return;
	}

	if (mt->config != NULL) {
		gc = calloc(sizeof *gc, 1);
		gc->name = strdup(name);
		gc->val = p;
		LIST_INSERT_HEAD(mt->config, gc, config);
		return;
	}

	if (p != NULL) {
printf("<<<%s>>>\n", p);
		free(p);
	}

	if (!strcmp(name, "consumer") && mt->consumer != NULL) {
		mt->consumer = NULL;
		return;
	}
	if (!strcmp(name, "provider") && mt->provider != NULL) {
		mt->provider = NULL;
		return;
	}
	if (!strcmp(name, "geom") && mt->consumer != NULL) {
		return;
	}
	if (!strcmp(name, "geom") && mt->provider != NULL) {
		return;
	}
	if (!strcmp(name, "geom") && mt->geom != NULL) {
		mt->geom = NULL;
		return;
	}
	if (!strcmp(name, "class") && mt->geom != NULL) {
		return;
	}
	if (!strcmp(name, "class") && mt->class != NULL) {
		mt->class = NULL;
		return;
	}
}

static void
CharData(void *userData , const XML_Char *s , int len)
{
	struct mystate *mt;
	const char *b, *e;

	mt = userData;

	b = s;
	e = s + len - 1;
	while (isspace(*b) && b < e)
		b++;
	while (isspace(*e) && e > b)
		e--;
	if (e != b || (*b && !isspace(*b)))
		sbuf_bcat(mt->sbuf[mt->level], b, e - b + 1);
}

struct gident *
geom_lookupid(struct gmesh *gmp, void *id)
{
	struct gident *gip;

	for (gip = gmp->ident; gip->id != NULL; gip++)
		if (gip->id == id)
			return (gip);
	return (NULL);
}

int
geom_xml2tree(struct gmesh *gmp, char *p)
{
	XML_Parser parser;
	struct mystate *mt;
	struct gclass *cl;
	struct ggeom *ge;
	struct gprovider *pr;
	struct gconsumer *co;
	int i;

	memset(gmp, 0, sizeof *gmp);
	LIST_INIT(&gmp->class);
	parser = XML_ParserCreate(NULL);
	mt = calloc(1, sizeof *mt);
	if (mt == NULL)
		return (ENOMEM);
	mt->mesh = gmp;
	XML_SetUserData(parser, mt);
	XML_SetElementHandler(parser, StartElement, EndElement);
	XML_SetCharacterDataHandler(parser, CharData);
	i = XML_Parse(parser, p, strlen(p), 1);
	if (i != 1)
		return (-1);
	XML_ParserFree(parser);
	gmp->ident = calloc(sizeof *gmp->ident, mt->nident + 1);
	if (gmp->ident == NULL)
		return (ENOMEM);
	free(mt);
	i = 0;
	/* Collect all identifiers */
	LIST_FOREACH(cl, &gmp->class, class) {
		gmp->ident[i].id = cl->id;
		gmp->ident[i].ptr = cl;
		gmp->ident[i].what = ISCLASS;
		i++;
		LIST_FOREACH(ge, &cl->geom, geom) {
			gmp->ident[i].id = ge->id;
			gmp->ident[i].ptr = ge;
			gmp->ident[i].what = ISGEOM;
			i++;
			LIST_FOREACH(pr, &ge->provider, provider) {
				gmp->ident[i].id = pr->id;
				gmp->ident[i].ptr = pr;
				gmp->ident[i].what = ISPROVIDER;
				i++;
			}
			LIST_FOREACH(co, &ge->consumer, consumer) {
				gmp->ident[i].id = co->id;
				gmp->ident[i].ptr = co;
				gmp->ident[i].what = ISCONSUMER;
				i++;
			}
		}
	}
	/* Substitute all identifiers */
	LIST_FOREACH(cl, &gmp->class, class) {
		LIST_FOREACH(ge, &cl->geom, geom) {
			ge->class = geom_lookupid(gmp, ge->class)->ptr;
			LIST_FOREACH(pr, &ge->provider, provider) {
				pr->geom = geom_lookupid(gmp, pr->geom)->ptr;
			}
			LIST_FOREACH(co, &ge->consumer, consumer) {
				co->geom = geom_lookupid(gmp, co->geom)->ptr;
				if (co->provider != NULL)
					co->provider = 
					    geom_lookupid(gmp, co->provider)->ptr;
			}
		}
	}
	return (0);
}

int
geom_gettree(struct gmesh *gmp)
{
	char *p;
	int error;

	p = geom_getxml();
	error = geom_xml2tree(gmp, p);
	free(p);
	return (error);
}

static void 
delete_config(struct gconf *gp)
{
	struct gconfig *cf;

	for (;;) {
		cf = LIST_FIRST(gp);
		if (cf == NULL)
			return;
		LIST_REMOVE(cf, config);
		free(cf->name);
		free(cf->val);
		free(cf);
	}
}

void
geom_deletetree(struct gmesh *gmp)
{
	struct gclass *cl;
	struct ggeom *ge;
	struct gprovider *pr;
	struct gconsumer *co;

	free(gmp->ident);
	gmp->ident = NULL;
	for (;;) {
		cl = LIST_FIRST(&gmp->class);
		if (cl == NULL) 
			break;
		LIST_REMOVE(cl, class);
		delete_config(&cl->config);
		if (cl->name) free(cl->name);
		for (;;) {
			ge = LIST_FIRST(&cl->geom);
			if (ge == NULL) 
				break;
			LIST_REMOVE(ge, geom);
			delete_config(&ge->config);
			if (ge->name) free(ge->name);
			for (;;) {
				pr = LIST_FIRST(&ge->provider);
				if (pr == NULL) 
					break;
				LIST_REMOVE(pr, provider);
				delete_config(&pr->config);
				if (pr->name) free(pr->name);
				if (pr->mode) free(pr->mode);
				free(pr);
			}
			for (;;) {
				co = LIST_FIRST(&ge->consumer);
				if (co == NULL) 
					break;
				LIST_REMOVE(co, consumer);
				delete_config(&co->config);
				if (co->mode) free(co->mode);
				free(co);
			}
			free(ge);
		}
		free(cl);
	}
}
