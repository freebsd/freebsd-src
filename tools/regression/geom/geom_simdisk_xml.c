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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <strings.h>
#include <err.h>
#include <md5.h>
#include <sys/errno.h>
#include <geom/geom.h>
#include <string.h>
#include <ctype.h>
#include <bsdxml.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/queue.h>
#include <sys/sbuf.h>

#include "geom_simdisk.h"

void
g_simdisk_xml_save(const char *name, const char *file)
{
	struct g_geom *gp;
	struct simdisk_softc *sc;
	struct sector *dsp;
	int i, j;
	FILE *f;
	u_char *p;

	LIST_FOREACH(gp, &g_simdisk_class.geom, geom) {
		if (strcmp(name, gp->name))
			continue;
		sc = gp->softc;
		f = fopen(file, "w");
		if (f == NULL)
			err(1, file);
		fprintf(f, "<?xml version=\"1.0\"?>\n");
		fprintf(f, "<DISKIMAGE>\n");
#if 0
		{
		struct sbuf *sb;
		sb = g_conf_specific(&g_simdisk_class, gp, NULL, NULL);
		fprintf(f, "  <config>%s</config>\n", sbuf_data(sb));
		}
#endif
		fprintf(f, "  <sectorsize>%u</sectorsize>\n", sc->sectorsize);
		fprintf(f, "  <mediasize>%llu</mediasize>\n", sc->mediasize);
		fprintf(f, "  <fwsectors>%u</fwsectors>\n", sc->fwsectors);
		fprintf(f, "  <fwheads>%u</fwheads>\n", sc->fwheads);
		fprintf(f, "  <fwcylinders>%u</fwcylinders>\n", sc->fwcylinders);
		LIST_FOREACH(dsp, &sc->sectors, sectors) {
			fprintf(f, "  <sector>\n");
			fprintf(f, "    <offset>%llu</offset>\n", dsp->offset);
			fprintf(f, "    <hexdata>\n");
			p = dsp->data;
			for (j = 0 ; j < sc->sectorsize; j += 32) {
				fprintf(f, "\t");
				for (i = 0; i < 32; i++)
					fprintf(f, "%02x", *p++);
				fprintf(f, "\n");
			}
			fprintf(f, "    </hexdata>\n");
			fprintf(f, "  </sector>\n");
		}
		fprintf(f, "</DISKIMAGE>\n");
		fclose(f);
	}
}

static void
startElement(void *userData, const char *name, const char **atts __unused)
{
	struct simdisk_softc *sc;

	sc = userData;
	if (!strcasecmp(name, "sector")) {
		sc->sp = calloc(1, sizeof(*sc->sp) + sc->sectorsize);
		sc->sp->data = (u_char *)(sc->sp + 1);
	}
	sbuf_clear(sc->sbuf);
}

static void
endElement(void *userData, const char *name)
{
	struct simdisk_softc *sc;
	char *p;
	u_char *q;
	int i, j;
	off_t o;

	sc = userData;

	if (!strcasecmp(name, "comment")) {
		sbuf_clear(sc->sbuf);
		return;
	}
	sbuf_finish(sc->sbuf);
	if (!strcasecmp(name, "sectorsize")) {
		sc->sectorsize = strtoul(sbuf_data(sc->sbuf), &p, 0);
		if (*p != '\0')
			errx(1, "strtoul croaked on sectorsize");
	} else if (!strcasecmp(name, "mediasize")) {
		o = strtoull(sbuf_data(sc->sbuf), &p, 0);
		if (*p != '\0')
			errx(1, "strtoul croaked on mediasize");
		if (o > 0)
			sc->mediasize = o;
	} else if (!strcasecmp(name, "fwsectors")) {
		sc->fwsectors = strtoul(sbuf_data(sc->sbuf), &p, 0);
		if (*p != '\0')
			errx(1, "strtoul croaked on fwsectors");
	} else if (!strcasecmp(name, "fwheads")) {
		sc->fwheads = strtoul(sbuf_data(sc->sbuf), &p, 0);
		if (*p != '\0')
			errx(1, "strtoul croaked on fwheads");
	} else if (!strcasecmp(name, "fwcylinders")) {
		sc->fwcylinders = strtoul(sbuf_data(sc->sbuf), &p, 0);
		if (*p != '\0')
			errx(1, "strtoul croaked on fwcylinders");
	} else if (!strcasecmp(name, "offset")) {
		sc->sp->offset= strtoull(sbuf_data(sc->sbuf), &p, 0);
		if (*p != '\0')
			errx(1, "strtoul croaked on offset");
	} else if (!strcasecmp(name, "fill")) {
		j = strtoul(sbuf_data(sc->sbuf), NULL, 16);
		memset(sc->sp->data, j, sc->sectorsize);
	} else if (!strcasecmp(name, "hexdata")) {
		q = sc->sp->data;
		p = sbuf_data(sc->sbuf);
		for (i = 0; i < sc->sectorsize; i++) {
			if (!isxdigit(*p))
				errx(1, "I croaked on hexdata %d:(%02x)", i, *p);
			if (isdigit(*p))
				j = (*p - '0') << 4;
			else
				j = (tolower(*p) - 'a' + 10) << 4;
			p++;
			if (!isxdigit(*p))
				errx(1, "I croaked on hexdata %d:(%02x)", i, *p);
			if (isdigit(*p))
				j |= *p - '0';
			else
				j |= tolower(*p) - 'a' + 10;
			p++;
			*q++ = j;
		}
	} else if (!strcasecmp(name, "sector")) {
		g_simdisk_insertsector(sc, sc->sp);
		sc->sp = NULL;
	} else if (!strcasecmp(name, "diskimage")) {
	} else if (!strcasecmp(name, "FreeBSD")) {
	} else {
		printf("<%s>[[%s]]\n", name, sbuf_data(sc->sbuf));
	}
	sbuf_clear(sc->sbuf);
}

static void
characterData(void *userData, const XML_Char *s, int len)
{
	const char *b, *e;
	struct simdisk_softc *sc;

	sc = userData;
	b = s;
	e = s + len - 1;
	while (isspace(*b) && b < e)
		b++;
	while (isspace(*e) && e > b)
		e--;
	if (e != b || !isspace(*b))
		sbuf_bcat(sc->sbuf, b, e - b + 1);
}

struct g_geom *
g_simdisk_xml_load(const char *name, const char *file)
{
	XML_Parser parser = XML_ParserCreate(NULL);
	struct stat st;
	char *p;
	struct simdisk_softc *sc;
	int fd, i;

	sc = calloc(1, sizeof *sc);
	sc->fd = -1;
	sc->sbuf = sbuf_new(NULL, NULL, 0, SBUF_AUTOEXTEND);
	sc->mediasize = 1024 * 1024 * 1024 * (off_t)1024;
	LIST_INIT(&sc->sectors);
	TAILQ_INIT(&sc->sort);
	XML_SetUserData(parser, sc);
	XML_SetElementHandler(parser, startElement, endElement);
	XML_SetCharacterDataHandler(parser, characterData);

	fd = open(file, O_RDONLY);
	if (fd < 0)
		err(1, file);
	fstat(fd, &st);
	p = mmap(NULL, st.st_size, PROT_READ, MAP_NOCORE|MAP_PRIVATE, fd, 0);
	i = XML_Parse(parser, p, st.st_size, 1);
	if (i != 1)
		errx(1, "XML_Parse complains: return %d", i);
	munmap(p, st.st_size);
	close(fd);
	XML_ParserFree(parser);
	return (g_simdisk_create(name, sc));
}
