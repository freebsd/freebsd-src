/*-
 * Copyright (c) 1999 Andrzej Bialecki <abial@freebsd.org>
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
 * $FreeBSD: src/release/sysinstall/kget.c,v 1.14.2.1 2000/07/14 10:21:32 jhb Exp $
 */

#ifdef __alpha__
int
kget(char *out)
{
    return -1;
}

#else

#include "sysinstall.h"
#include <sys/sysctl.h>
#include <machine/uc_device.h>

int
kget(char *out)
{
    int len, i, bytes_written = 0;
    char *buf;
    char *mib1 = "machdep.uc_devlist";
    char name[9];
    FILE *fout = NULL;
    struct uc_device *id;
    char *p;
 
    /* create the output file; if we end up not writing to it, we'll 
       unlink() it later. */
    fout = fopen(out, "w");
    if (fout == NULL) {
	msgDebug("kget: Unable to open %s for writing.\n", out);
	return -1;
    }

    /* We use sysctlbyname, because the oid is unknown (OID_AUTO) */
    /* get the buffer size */
    i = sysctlbyname(mib1, NULL, &len, NULL, NULL);
    if (i) {
	msgDebug("kget: error buffer sizing\n");
	goto bail;
    }
    if (len <= 0) {
	msgDebug("kget: mib1 has length of %d\n", len);
	goto bail;
    }
    buf = (char *)alloca(len * sizeof(char));
    i = sysctlbyname(mib1, buf, &len, NULL, NULL);
    if (i) {
	msgDebug("kget: error retrieving data\n");
	goto bail;
    }


    i = 0;
    while (i < len) {
	id = (struct uc_device *)(buf + i);
	p = (buf + i + sizeof(struct uc_device));
	strncpy(name, p, 8);
	if (!id->id_enabled) {
	    bytes_written += fprintf(fout, "di %s%d\n", name, id->id_unit);
	}
	else {
	    bytes_written += fprintf(fout, "en %s%d\n", name, id->id_unit);
	    if (id->id_iobase > 0) {
		bytes_written += fprintf(fout, "po %s%d %#x\n",
					 name, id->id_unit, id->id_iobase);
	    }
	    if (id->id_irq > 0) {
		bytes_written += fprintf(fout, "ir %s%d %d\n", name,
					 id->id_unit, ffs(id->id_irq) - 1);
	    }
	    if (id->id_drq > 0) {
		bytes_written += fprintf(fout, "dr %s%d %d\n", name,
					 id->id_unit, id->id_drq);
	    }
	    if (id->id_maddr > 0) {
		bytes_written += fprintf(fout, "iom %s%d %#x\n", name,
					 id->id_unit, (u_int)id->id_maddr);
	    }
	    if (id->id_msize > 0) {
		bytes_written += fprintf(fout, "ios %s%d %d\n", name,
					 id->id_unit, id->id_msize);
	    }
	    bytes_written += fprintf(fout, "f %s%d %#x\n", name,
				     id->id_unit, id->id_flags);
	}
	i += sizeof(struct uc_device) + 8;
    }

bail:
    if (bytes_written)
	fprintf(fout, "q\n");
    else
	unlink(out);
    fclose(fout);
    return 0;
}

#endif	/* !alpha */
