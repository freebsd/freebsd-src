/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2002 Poul-Henning Kamp
 * Copyright (c) 2002 Networks Associates Technology, Inc.
 * All rights reserved.
 * Copyright (c) 2013-2022 Alexander Motin <mav@FreeBSD.org>
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
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/sbuf.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <machine/stdarg.h>

#include <geom/geom.h>
#include <geom/geom_int.h>
#include <geom/geom_disk.h>

static void
g_confdot_consumer(struct sbuf *sb, struct g_consumer *cp)
{

	sbuf_printf(sb, "z%p [label=\"r%dw%de%d\"];\n",
	    cp, cp->acr, cp->acw, cp->ace);
	if (cp->provider)
		sbuf_printf(sb, "z%p -> z%p;\n", cp, cp->provider);
}

static void
g_confdot_provider(struct sbuf *sb, struct g_provider *pp)
{

	sbuf_printf(sb, "z%p [shape=hexagon,label=\"%s\\nr%dw%de%d\\nerr#%d\\n"
	    "sector=%u\\nstripe=%ju\"];\n", pp, pp->name, pp->acr, pp->acw,
	    pp->ace, pp->error, pp->sectorsize, (uintmax_t)pp->stripesize);
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
g_confdot(void *p, int flag )
{
	struct g_class *mp;
	struct sbuf *sb;

	KASSERT(flag != EV_CANCEL, ("g_confdot was cancelled"));
	sb = p;
	g_topology_assert();
	sbuf_cat(sb, "digraph geom {\n");
	LIST_FOREACH(mp, &g_classes, class)
		g_confdot_class(sb, mp);
	sbuf_cat(sb, "}\n");
	sbuf_finish(sb);
}

static void
g_conftxt_geom(struct sbuf *sb, struct g_geom *gp, int level)
{
	struct g_provider *pp;
	struct g_consumer *cp;

	if (gp->flags & G_GEOM_WITHER)
		return;
	LIST_FOREACH(pp, &gp->provider, provider) {
		sbuf_printf(sb, "%d %s %s %ju %u", level, gp->class->name,
		    pp->name, (uintmax_t)pp->mediasize, pp->sectorsize);
		if (gp->dumpconf != NULL)
			gp->dumpconf(sb, NULL, gp, NULL, pp);
		sbuf_cat(sb, "\n");
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
g_conftxt(void *p, int flag)
{
	struct g_class *mp;
	struct sbuf *sb;

	KASSERT(flag != EV_CANCEL, ("g_conftxt was cancelled"));
	sb = p;
	g_topology_assert();
	LIST_FOREACH(mp, &g_classes, class) {
		if (!strcmp(mp->name, G_DISK_CLASS_NAME) || !strcmp(mp->name, "MD"))
			g_conftxt_class(sb, mp);
	}
	sbuf_finish(sb);
}

void
g_conf_cat_escaped(struct sbuf *sb, const char *buf)
{
	const u_char *c;

	for (c = buf; *c != '\0'; c++) {
		if (*c == '&' || *c == '<' || *c == '>' ||
		    *c == '\'' || *c == '"' || *c > 0x7e)
			sbuf_printf(sb, "&#x%X;", *c);
		else if (*c == '\t' || *c == '\n' || *c == '\r' || *c > 0x1f)
			sbuf_putc(sb, *c);
		else
			sbuf_putc(sb, '?');
	}
}

void
g_conf_printf_escaped(struct sbuf *sb, const char *fmt, ...)
{
	struct sbuf *s;
	va_list ap;

	s = sbuf_new_auto();
	va_start(ap, fmt);
	sbuf_vprintf(s, fmt, ap);
	va_end(ap);
	sbuf_finish(s);

	g_conf_cat_escaped(sb, sbuf_data(s));
	sbuf_delete(s);
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
	if (cp->geom->flags & G_GEOM_WITHER)
		;
	else if (cp->geom->dumpconf != NULL) {
		sbuf_cat(sb, "\t  <config>\n");
		cp->geom->dumpconf(sb, "\t    ", cp->geom, cp, NULL);
		sbuf_cat(sb, "\t  </config>\n");
	}
	sbuf_cat(sb, "\t</consumer>\n");
}

static void
g_conf_provider(struct sbuf *sb, struct g_provider *pp)
{
	struct g_geom_alias *gap;

	sbuf_printf(sb, "\t<provider id=\"%p\">\n", pp);
	sbuf_printf(sb, "\t  <geom ref=\"%p\"/>\n", pp->geom);
	sbuf_printf(sb, "\t  <mode>r%dw%de%d</mode>\n",
	    pp->acr, pp->acw, pp->ace);
	sbuf_cat(sb, "\t  <name>");
	g_conf_cat_escaped(sb, pp->name);
	sbuf_cat(sb, "</name>\n");
	LIST_FOREACH(gap, &pp->aliases, ga_next) {
		sbuf_cat(sb, "\t  <alias>");
		g_conf_cat_escaped(sb, gap->ga_alias);
		sbuf_cat(sb, "</alias>\n");
	}
	sbuf_printf(sb, "\t  <mediasize>%jd</mediasize>\n",
	    (intmax_t)pp->mediasize);
	sbuf_printf(sb, "\t  <sectorsize>%u</sectorsize>\n", pp->sectorsize);
	sbuf_printf(sb, "\t  <stripesize>%ju</stripesize>\n", (uintmax_t)pp->stripesize);
	sbuf_printf(sb, "\t  <stripeoffset>%ju</stripeoffset>\n", (uintmax_t)pp->stripeoffset);
	if (pp->flags & G_PF_WITHER)
		sbuf_cat(sb, "\t  <wither/>\n");
	else if (pp->geom->flags & G_GEOM_WITHER)
		;
	else if (pp->geom->dumpconf != NULL) {
		sbuf_cat(sb, "\t  <config>\n");
		pp->geom->dumpconf(sb, "\t    ", pp->geom, NULL, pp);
		sbuf_cat(sb, "\t  </config>\n");
	}
	sbuf_cat(sb, "\t</provider>\n");
}

static void
g_conf_geom(struct sbuf *sb, struct g_geom *gp)
{
	struct g_consumer *cp;
	struct g_provider *pp;

	sbuf_printf(sb, "    <geom id=\"%p\">\n", gp);
	sbuf_printf(sb, "      <class ref=\"%p\"/>\n", gp->class);
	sbuf_cat(sb, "      <name>");
	g_conf_cat_escaped(sb, gp->name);
	sbuf_cat(sb, "</name>\n");
	sbuf_printf(sb, "      <rank>%d</rank>\n", gp->rank);
	if (gp->flags & G_GEOM_WITHER)
		sbuf_cat(sb, "      <wither/>\n");
	else if (gp->dumpconf != NULL) {
		sbuf_cat(sb, "      <config>\n");
		gp->dumpconf(sb, "\t", gp, NULL, NULL);
		sbuf_cat(sb, "      </config>\n");
	}
	LIST_FOREACH(cp, &gp->consumer, consumer)
		g_conf_consumer(sb, cp);
	LIST_FOREACH(pp, &gp->provider, provider)
		g_conf_provider(sb, pp);
	sbuf_cat(sb, "    </geom>\n");
}

static bool
g_conf_matchgp(struct g_geom *gp, struct g_geom **gps)
{

	if (gps == NULL)
		return (true);
	for (; *gps != NULL; gps++) {
		if (*gps == gp)
			return (true);
	}
	return (false);
}

static void
g_conf_class(struct sbuf *sb, struct g_class *mp, struct g_geom **gps)
{
	struct g_geom *gp;

	sbuf_printf(sb, "  <class id=\"%p\">\n", mp);
	sbuf_cat(sb, "    <name>");
	g_conf_cat_escaped(sb, mp->name);
	sbuf_cat(sb, "</name>\n");
	LIST_FOREACH(gp, &mp->geom, geom) {
		if (!g_conf_matchgp(gp, gps))
			continue;
		g_conf_geom(sb, gp);
		if (sbuf_error(sb))
			break;
	}
	sbuf_cat(sb, "  </class>\n");
}

void
g_conf_specific(struct sbuf *sb, struct g_geom **gps)
{
	struct g_class *mp2;

	g_topology_assert();
	sbuf_cat(sb, "<mesh>\n");
	LIST_FOREACH(mp2, &g_classes, class) {
		g_conf_class(sb, mp2, gps);
		if (sbuf_error(sb))
			break;
	}
	sbuf_cat(sb, "</mesh>\n");
	sbuf_finish(sb);
}

void
g_confxml(void *p, int flag)
{

	KASSERT(flag != EV_CANCEL, ("g_confxml was cancelled"));
	g_topology_assert();
	g_conf_specific(p, NULL);
}

void
(g_trace)(int level, const char *fmt, ...)
{
	va_list ap;

	if (!(g_debugflags & level))
		return;
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	printf("\n");
}
