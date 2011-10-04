/*-
 * Copyright (c) 2011 Lev Serebryakov
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/ctype.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/malloc.h>
#include <sys/bio.h>
#include <sys/ktr.h>
#include <sys/proc.h>
#include <sys/stack.h>
#include <sys/errno.h>
#include <machine/stdarg.h>
#include <geom/geom.h>
#include <geom/geom_int.h>

#include <vm/uma.h>

static void
g_devctl_notify(struct g_provider *pp, const char *event, const char *format, ...)
{
	char *full_data;
	char *additional_data;
	char *subsystem;
	va_list args;
	int len;
	int i;

	subsystem = g_malloc(strlen(pp->geom->class->name) + 1, M_WAITOK);
	if (subsystem == NULL)
		return;
	for (i = 0; pp->geom->class->name[i]; i++)
		subsystem[i] = tolower(pp->geom->class->name[i]);
	subsystem[i] = 0;

	if (format != NULL) {
		additional_data = g_malloc(1024, M_WAITOK);
		if (additional_data == NULL) {
			g_free(subsystem);
			return;
		}
		va_start(args, format);
		len = vsnprintf(additional_data, 1024, format, args);
		va_end(args);
	} else {
		additional_data = NULL;
		len = 0;
	}

	/* Add more space for mandatory arguments, space and zero */
	len += sizeof("geom=") + strlen(pp->geom->name) +
	       sizeof("provider=") + strlen(pp->name) + 2;
	full_data = g_malloc(len, M_WAITOK);
	if (full_data == NULL) {
		g_free(subsystem);
		if (additional_data != NULL)
			g_free(additional_data);
		return;
	}

	if (additional_data != NULL)
		snprintf(full_data, len, "geom=%s provider=%s %s",
		    pp->geom->name, pp->name, additional_data);
	else
		snprintf(full_data, len, "geom=%s provider=%s",
		    pp->geom->name, pp->name);

	devctl_notify("GEOM", subsystem, event, full_data);
	g_free(subsystem);
	if (additional_data != NULL)
		g_free(additional_data);
	g_free(full_data);
}


void
g_notify_disconnect(struct g_provider *pp, struct g_consumer *cp, char state)
{
	g_devctl_notify(pp, "DISCONNECT", "disk=%s state=%c",
		(cp != NULL && cp->provider != NULL && cp->provider->name != NULL)?
			cp->provider->name:"[unknown]",
		 state);
}

void
g_notify_sync_start(struct g_provider *pp)
{
	g_devctl_notify(pp, "SYNCSTART", NULL);
}

void
g_notify_sync_stop(struct g_provider *pp, boolean_t complete)
{
	g_devctl_notify(pp, "SYNCSTOP", "complete=%c", complete?'Y':'N');
}

void
g_notify_destroyed(struct g_provider *pp)
{
	g_devctl_notify(pp, "DESTROYED", NULL);
}
