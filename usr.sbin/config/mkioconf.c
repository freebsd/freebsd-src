/*
 * Copyright (c) 1980, 1993
 *	The Regents of the University of California.  All rights reserved.
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

#ifndef lint
#if 0
static char sccsid[] = "@(#)mkioconf.c	8.2 (Berkeley) 1/21/94";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <err.h>
#include <stdio.h>
#include "y.tab.h"
#include "config.h"

/*
 * build the ioconf.c file
 */

static char *
devstr(struct device *dp)
{
    static char buf[100];

    if (dp->d_unit >= 0) {
	snprintf(buf, sizeof(buf), "%s%d", dp->d_name, dp->d_unit);
	return buf;
    } else
	return dp->d_name;
}

static void
write_device_resources(FILE *fp, struct device *dp)
{
    int count = 0;
    char buf[80];

    fprintf(fp, "struct config_resource %s_resources[] = {\n", devstr(dp));
    if (dp->d_conn) {
	if (dp->d_connunit >= 0)
	    snprintf(buf, sizeof(buf), "%s%d", dp->d_conn, dp->d_connunit);
	else
	    snprintf(buf, sizeof(buf), "%s", dp->d_conn);
	fprintf(fp, "\t{ \"at\",\tRES_STRING,\t{ (long)\"%s\" }},\n", buf);
	count++;
    }
    if (dp->d_drive != -2) {
	fprintf(fp, "\t{ \"drive\",\tRES_INT,\t{ %d }},\n", dp->d_drive);
	count++;
    }
    if (dp->d_target != -2) {
	fprintf(fp, "\t{ \"target\",\tRES_INT,\t{ %d }},\n", dp->d_target);
	count++;
    }
    if (dp->d_lun != -2) {
	fprintf(fp, "\t{ \"lun\",\tRES_INT,\t{ %d }},\n", dp->d_lun);
	count++;
    }
    if (dp->d_bus != -2) {
	fprintf(fp, "\t{ \"bus\",\tRES_INT,\t{ %d }},\n", dp->d_bus);
	count++;
    }
    if (dp->d_flags) {
	fprintf(fp, "\t{ \"flags\",\tRES_INT,\t{ 0x%x }},\n", dp->d_flags);
	count++;
    }
    if (dp->d_disabled) {
	fprintf(fp, "\t{ \"disabled\",\tRES_INT,\t{ %d }},\n", dp->d_disabled);
	count++;
    }
    if (dp->d_port) {
	fprintf(fp, "\t{ \"port\",\tRES_INT,\t { %s }},\n", dp->d_port);
	count++;
    }
    if (dp->d_portn > 0) {
	fprintf(fp, "\t{ \"port\",\tRES_INT,\t{ 0x%x }},\n", dp->d_portn);
	count++;
    }
    if (dp->d_maddr > 0) {
	fprintf(fp, "\t{ \"maddr\",\tRES_INT,\t{ 0x%x }},\n", dp->d_maddr);
	count++;
    }
    if (dp->d_msize > 0) {
	fprintf(fp, "\t{ \"msize\",\tRES_INT,\t{ 0x%x }},\n", dp->d_msize);
	count++;
    }
    if (dp->d_drq >= 0) {
	fprintf(fp, "\t{ \"drq\",\tRES_INT,\t{ %d }},\n", dp->d_drq);
	count++;
    }
    if (dp->d_irq > 0) {
	fprintf(fp, "\t{ \"irq\",\tRES_INT,\t{ %d }},\n", dp->d_irq);
	count++;
    }
    fprintf(fp, "};\n");
    fprintf(fp, "#define %s_count %d\n", devstr(dp), count);
}

static void
write_all_device_resources(FILE *fp)
{
	struct device *dp;

	for (dp = dtab; dp != 0; dp = dp->d_next) {
		if (dp->d_type != DEVICE)
			continue;
		if (dp->d_unit == UNKNOWN)
			continue;
		write_device_resources(fp, dp);
	}
}

static void
write_devtab(FILE *fp)
{
	struct device *dp;
	int count;

	write_all_device_resources(fp);

	count = 0;
	fprintf(fp, "struct config_device config_devtab[] = {\n");
	for (dp = dtab; dp != 0; dp = dp->d_next) {
		char* n = devstr(dp);
		if (dp->d_type != DEVICE)
			continue;
		if (dp->d_unit == UNKNOWN)
			continue;
		fprintf(fp, "\t{ \"%s\",\t%d,\t%s_count,\t%s_resources },\n",
			dp->d_name, dp->d_unit, n, n);
		count++;
	}
	fprintf(fp, "};\n");
	fprintf(fp, "int devtab_count = %d;\n", count);
}

void
newbus_ioconf()
{
	FILE *fp;

	fp = fopen(path("ioconf.c.new"), "w");
	if (fp == 0)
		err(1, "%s", path("ioconf.c.new"));
	fprintf(fp, "/*\n");
	fprintf(fp, " * I/O configuration.\n");
	fprintf(fp, " * DO NOT EDIT-- this file is automatically generated.\n");
	fprintf(fp, " */\n");
	fprintf(fp, "\n");
	fprintf(fp, "#include <sys/param.h>\n");
	fprintf(fp, "\n");
	fprintf(fp, "/*\n");
	fprintf(fp, " * New bus architecture devices.\n");
	fprintf(fp, " */\n");
	fprintf(fp, "\n");
	fprintf(fp, "#include <sys/queue.h>\n");
	fprintf(fp, "#include <sys/sysctl.h>\n");
	if (machine == MACHINE_PC98)
		fprintf(fp, "#include <pc98/pc98/pc98.h>\n");
	else
		fprintf(fp, "#include <isa/isareg.h>\n");
	fprintf(fp, "#include <sys/bus_private.h>\n");
	fprintf(fp, "\n");

	write_devtab(fp);

	(void) fclose(fp);
	moveifchanged(path("ioconf.c.new"), path("ioconf.c"));
}
