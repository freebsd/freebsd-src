/*-
 * Copyright (c) 2002 Poul-Henning Kamp
 * Copyright (c) 2002 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by Poul-Henning Kamp
 * and NAI Labs, the Security Research Division of Network Associates, Inc.
 * under DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"), as part of the
 * DARPA CHATS research program.
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


#include <sys/param.h>
#include <sys/stdint.h>
#include <sys/sbuf.h>
#ifndef _KERNEL
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <err.h>
#else
#include <sys/systm.h>
#include <sys/malloc.h>
#endif
#include <machine/stdarg.h>

#include <geom/geom.h>
#include <geom/geom_int.h>


static void
g_confdot_consumer(struct sbuf *sb, struct g_consumer *cp)
{

	sbuf_printf(sb, "z%p [label=\"r%dw%de%d\\nbio #%d\"];\n",
	    cp, cp->acr, cp->acw, cp->ace, cp->biocount);
	if (cp->provider)
		sbuf_printf(sb, "z%p -> z%p;\n", cp, cp->provider);
}

static void
g_confdot_provider(struct sbuf *sb, struct g_provider *pp)
{

	sbuf_printf(sb, "z%p [shape=hexagon,label=\"%s\\nr%dw%de%d\\nerr#%d\"];\n",
	    pp, pp->name, pp->acr, pp->acw, pp->ace, pp->error);
}

static void
g_confdot_geom(struct sbuf *sb, struct g_geom *gp)
{
	struct g_consumer *cp;
	struct g_provider *pp;

	sbuf_printf(sb, "z%p [shape=box,label=\"%s\\n%s\\nr#%d\"];\n",
	    gp, gp->class->name, gp->name, gp->rank);
	LIST_FOREACH(cp, &gp->consumer, consumer) {
		g_confdot_consumer(sb, cp);
		sbuf_printf(sb, "z%p -> z%p;\n", gp, cp);
	}

	LIST_FOREACH(pp, &gp->provider, provider) {
		g_confdot_provider(sb, pp);
		sbuf_printf(sb, "z%p -> z%p;\n", pp, gp);
	}
}

static void
g_confdot_class(struct sbuf *sb, struct g_class *mp)
{
	struct g_geom *gp;

	LIST_FOREACH(gp, &mp->geom, geom)
		g_confdot_geom(sb, gp);
}

void
g_confdot(void *p)
{
	struct g_class *mp;
	struct sbuf *sb;

	sb = p;
	g_topology_assert();
	sbuf_printf(sb, "digraph geom {\n");
	LIST_FOREACH(mp, &g_classes, class)
		g_confdot_class(sb, mp);
	sbuf_printf(sb, "};\n");
	sbuf_finish(sb);
	wakeup(p);
}

static void
g_conftxt_geom(struct sbuf *sb, struct g_geom *gp, int level)
{
	struct g_provider *pp;
	struct g_consumer *cp;

	LIST_FOREACH(pp, &gp->provider, provider) {
		sbuf_printf(sb, "%d %s %s %ju %u", level, gp->class->name,
		    pp->name, (uintmax_t)pp->mediasize, pp->sectorsize);
		gp->dumpconf(sb, NULL, gp, NULL, pp);
		sbuf_printf(sb, "\n");
		LIST_FOREACH(cp, &pp->consumers, consumers)
			g_conftxt_geom(sb, cp->geom, level + 1);
	}
}

static void
g_conftxt_class(struct sbuf *sb, struct g_class *mp)
{
	struct g_geom *gp;

	LIST_FOREACH(gp, &mp->geom, geom)
		g_conftxt_geom(sb, gp, 0);
}

void
g_conftxt(void *p)
{
	struct g_class *mp;
	struct sbuf *sb;

	sb = p;
	g_topology_assert();
	LIST_FOREACH(mp, &g_classes, class)
		if (!strcmp(mp->name, "DISK"))
			break;
	if (mp != NULL)
		g_conftxt_class(sb, mp);
	else
		printf("no DISK\n");
	sbuf_finish(sb);
	wakeup(p);
}


static void
g_conf_consumer(struct sbuf *sb, struct g_consumer *cp)
{

	sbuf_printf(sb, "\t<consumer id=\"%p\">\n", cp);
	sbuf_printf(sb, "\t  <geom ref=\"%p\"/>\n", cp->geom);
	if (cp->provider != NULL)
		sbuf_printf(sb, "\t  <provider ref=\"%p\"/>\n", cp->provider);
	sbuf_printf(sb, "\t  <mode>r%dw%de%d</mode>\n",
	    cp->acr, cp->acw, cp->ace);
	if (cp->geom->dumpconf) {
		sbuf_printf(sb, "\t  <config>\n");
		cp->geom->dumpconf(sb, "\t    ", cp->geom, cp, NULL);
		sbuf_printf(sb, "\t  </config>\n");
	}
	sbuf_printf(sb, "\t</consumer>\n");
}

static void
g_conf_provider(struct sbuf *sb, struct g_provider *pp)
{

	sbuf_printf(sb, "\t<provider id=\"%p\">\n", pp);
	sbuf_printf(sb, "\t  <geom ref=\"%p\"/>\n", pp->geom);
	sbuf_printf(sb, "\t  <mode>r%dw%de%d</mode>\n",
	    pp->acr, pp->acw, pp->ace);
	sbuf_printf(sb, "\t  <name>%s</name>\n", pp->name);
	sbuf_printf(sb, "\t  <mediasize>%jd</mediasize>\n",
	    (intmax_t)pp->mediasize);
	sbuf_printf(sb, "\t  <sectorsize>%u</sectorsize>\n", pp->sectorsize);
	if (pp->geom->dumpconf) {
		sbuf_printf(sb, "\t  <config>\n");
		pp->geom->dumpconf(sb, "\t    ", pp->geom, NULL, pp);
		sbuf_printf(sb, "\t  </config>\n");
	}
	sbuf_printf(sb, "\t</provider>\n");
}


static void
g_conf_geom(struct sbuf *sb, struct g_geom *gp, struct g_provider *pp, struct g_consumer *cp)
{
	struct g_consumer *cp2;
	struct g_provider *pp2;

	sbuf_printf(sb, "    <geom id=\"%p\">\n", gp);
	sbuf_printf(sb, "      <class ref=\"%p\"/>\n", gp->class);
	sbuf_printf(sb, "      <name>%s</name>\n", gp->name);
	sbuf_printf(sb, "      <rank>%d</rank>\n", gp->rank);
	if (gp->dumpconf) {
		sbuf_printf(sb, "      <config>\n");
		gp->dumpconf(sb, "\t", gp, NULL, NULL);
		sbuf_printf(sb, "      </config>\n");
	}
	LIST_FOREACH(cp2, &gp->consumer, consumer) {
		if (cp != NULL && cp != cp2)
			continue;
		g_conf_consumer(sb, cp2);
	}

	LIST_FOREACH(pp2, &gp->provider, provider) {
		if (pp != NULL && pp != pp2)
			continue;
		g_conf_provider(sb, pp2);
	}
	sbuf_printf(sb, "    </geom>\n");
}

static void
g_conf_class(struct sbuf *sb, struct g_class *mp, struct g_geom *gp, struct g_provider *pp, struct g_consumer *cp)
{
	struct g_geom *gp2;

	sbuf_printf(sb, "  <class id=\"%p\">\n", mp);
	sbuf_printf(sb, "    <name>%s</name>\n", mp->name);
	LIST_FOREACH(gp2, &mp->geom, geom) {
		if (gp != NULL && gp != gp2)
			continue;
		g_conf_geom(sb, gp2, pp, cp);
	}
	sbuf_printf(sb, "  </class>\n");
}

void
g_conf_specific(struct sbuf *sb, struct g_class *mp, struct g_geom *gp, struct g_provider *pp, struct g_consumer *cp)
{
	struct g_class *mp2;

	g_topology_assert();
	sbuf_printf(sb, "<mesh>\n");
#ifndef _KERNEL
	sbuf_printf(sb, "  <FreeBSD>%cFreeBSD%c</FreeBSD>\n", '$', '$');
#endif
	LIST_FOREACH(mp2, &g_classes, class) {
		if (mp != NULL && mp != mp2)
			continue;
		g_conf_class(sb, mp2, gp, pp, cp);
	}
	sbuf_printf(sb, "</mesh>\n");
	sbuf_finish(sb);
}

void
g_confxml(void *p)
{

	g_topology_assert();
	g_conf_specific(p, NULL, NULL, NULL, NULL);
	wakeup(p);
}

void
g_trace(int level, char *fmt, ...)
{
	va_list ap;

	g_sanity(NULL);
	if (!(g_debugflags & level))
		return;
	va_start(ap, fmt);
	vprintf(fmt, ap);
	printf("\n");
}

void
g_hexdump(void *ptr, int length)
{
	int i, j, k;
	unsigned char *cp;

	cp = ptr;
	for (i = 0; i < length; i+= 16) {
		printf("%04x  ", i);
		for (j = 0; j < 16; j++) {
			k = i + j;
			if (k < length)
				printf(" %02x", cp[k]);
			else
				printf("   ");
		}
		printf("  |");
		for (j = 0; j < 16; j++) {
			k = i + j;
			if (k >= length)
				printf(" ");
			else if (cp[k] >= ' ' && cp[k] <= '~')
				printf("%c", cp[k]);
			else
				printf(".");
		}
		printf("|\n");
	}
}

