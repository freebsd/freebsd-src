/*-
 * Copyright (c) 1998 Michael Smith
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
 *	$Id$
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/linker.h>

/*
 * Preloaded module support
 */

caddr_t	module_metadata;

/*
 * Search for the preloaded module (name)
 */
caddr_t
module_search_by_name(const char *name)
{
    caddr_t	curp;
    u_int32_t	*hdr;
    
    if (module_metadata != NULL) {
	
	curp = module_metadata;
	for (;;) {
	    hdr = (u_int32_t *)curp;
	    if (hdr[0] == 0)
		break;

	    /* Search for a MODINFO_NAME field */
	    if ((hdr[0] == MODINFO_NAME) &&
		!strcmp(name, curp + sizeof(u_int32_t) * 2))
		return(curp);

	    /* skip to next field */
	    curp += sizeof(u_int32_t) * 2 + hdr[1];
	}
    }
    return(NULL);
}

/*
 * Search for the first preloaded module of (type)
 */
caddr_t
module_search_by_type(const char *type)
{
    caddr_t	curp, lname;
    u_int32_t	*hdr;

    if (module_metadata != NULL) {

	curp = module_metadata;
	lname = NULL;
	for (;;) {
	    hdr = (u_int32_t *)curp;
	    if (hdr[0] == 0)
		break;

	    /* remember the start of each record */
	    if (hdr[0] == MODINFO_NAME)
		lname = curp;

	    /* Search for a MODINFO_TYPE field */
	    if ((hdr[0] == MODINFO_TYPE) &&
		!strcmp(type, curp + sizeof(u_int32_t) * 2))
		return(lname);

	    /* skip to next field */
	    curp += sizeof(u_int32_t) * 2 + hdr[1];
	}
    }
    return(NULL);
}

/*
 * Given a preloaded module handle (mod), return a pointer
 * to the data for the attribute (inf).
 */
caddr_t
module_search_info(caddr_t mod, int inf)
{
    caddr_t	curp;
    u_int32_t	*hdr;
    u_int32_t	type = 0;

    curp = mod;
    for (;;) {
	hdr = (u_int32_t *)curp;
	/* end of module data? */
	if (hdr[0] == 0)
	    break;
	/* 
	 * We give up once we've looped back to what we were looking at 
	 * first - this should normally be a MODINFO_NAME field.
	 */
	if (type == 0) {
	    type = hdr[0];
	} else {
	    if (hdr[0] == type)
		break;
	}
	
	/* 
	 * Attribute match? Return pointer to data.
	 * Consumer may safely assume that size value preceeds	
	 * data.
	 */
	if (hdr[0] == inf)
	    return(curp + (sizeof(u_int32_t) * 2));

	/* skip to next field */
	curp += sizeof(u_int32_t) * 2 + hdr[1];
    }
    return(NULL);
}

