/*-
 * Copyright (c) 2005 Sandvine Incorporated.  All righs reserved.
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
 * Author: Ed Maste <emaste@phaedrus.sandvine.ca>
 */

/*
 * This module detects Intel Multiprocessor spec info (mptable) and returns
 * the number of cpu's identified.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <machine/mptable.h>

#include <fcntl.h>
#include <inttypes.h>
#include <paths.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "sysinstall.h"

#define MPFPS_SIG "_MP_"
#define MPCTH_SIG "PCMP"

#define	PTOV(pa)	((off_t)(pa))

static mpfps_t biosmptable_find_mpfps(void);
static mpfps_t biosmptable_search_mpfps(off_t base, int length);
static mpcth_t biosmptable_check_mpcth(off_t addr);

static int memopen(void);
static void memclose(void);

int
biosmptable_detect(void)
{
    mpfps_t mpfps;
    mpcth_t mpcth;
    char *entry_type_p;
    proc_entry_ptr proc;
    int ncpu, i;

    if (!memopen())
	return -1;		/* XXX 0? */
    /* locate and validate the mpfps */
    mpfps = biosmptable_find_mpfps();
    mpcth = NULL;
    if (mpfps == NULL) {
	ncpu = 0;
    } else if (mpfps->config_type != 0) {
	/* 
	 * If thie config_type is nonzero then this is a default configuration
	 * from Chapter 5 in the MP spec.  Report 2 cpus and 1 I/O APIC.
	 */
    	ncpu = 2;
    } else {
	ncpu = 0;
	mpcth = biosmptable_check_mpcth(PTOV(mpfps->pap));
	if (mpcth != NULL) {
	    entry_type_p = (char *)(mpcth + 1);
	    for (i = 0; i < mpcth->entry_count; i++) {
		switch (*entry_type_p) {
		case 0:
		    entry_type_p += sizeof(struct PROCENTRY);
		    proc = (proc_entry_ptr) entry_type_p;
		    msgDebug("MPTable: Found CPU APIC ID %d %s\n",
			proc->apic_id,
			proc->cpu_flags & PROCENTRY_FLAG_EN ?
				"enabled" : "disabled");
		    if (proc->cpu_flags & PROCENTRY_FLAG_EN)
			ncpu++;
		    break;
		case 1:
		    entry_type_p += sizeof(struct BUSENTRY);
		    break;
		case 2:
		    entry_type_p += sizeof(struct IOAPICENTRY);
		    break;
		case 3:
		case 4:
		    entry_type_p += sizeof(struct INTENTRY);
		    break;
		default:
		    msgDebug("%s: unknown mptable entry type (%d)\n",
			__func__, *entry_type_p);
		    goto done;		/* XXX error return? */
		}
	    }
	done:
	    ;
	}
    }
    memclose();
    if (mpcth != NULL)
	free(mpcth);
    if (mpfps != NULL)
	free(mpfps);

    return ncpu;
}

static int pfd = -1;

static int
memopen(void)
{
    if (pfd < 0) {
	pfd = open(_PATH_MEM, O_RDONLY);
	if (pfd < 0)
		warn("%s: cannot open", _PATH_MEM);
    }
    return pfd >= 0;
}

static void
memclose(void)
{
    if (pfd >= 0) {
	close(pfd);
	pfd = -1;
    }
}

static int
memread(off_t addr, void* entry, size_t size)
{
    if ((size_t)pread(pfd, entry, size, addr) != size) {
	warn("pread (%lu @ 0x%lx)", size, addr);
	return 0;
    }
    return 1;
}


/*
 * Find the MP Floating Pointer Structure.  See the MP spec section 4.1.
 */
static mpfps_t
biosmptable_find_mpfps(void)
{
    mpfps_t mpfps;
    uint16_t addr;

    /* EBDA is the 1 KB addressed by the 16 bit pointer at 0x40E. */
    if (!memread(PTOV(0x40E), &addr, sizeof(addr)))
	return (NULL);
    mpfps = biosmptable_search_mpfps(PTOV(addr << 4), 0x400);
    if (mpfps != NULL)
	return (mpfps);

    /* Check the BIOS. */
    mpfps = biosmptable_search_mpfps(PTOV(0xf0000), 0x10000);
    if (mpfps != NULL)
	return (mpfps);

    return (NULL);
}

static mpfps_t
biosmptable_search_mpfps(off_t base, int length)
{
    mpfps_t mpfps;
    u_int8_t *cp, sum;
    int ofs, idx;

    mpfps = malloc(sizeof(*mpfps));
    if (mpfps == NULL) {
	msgDebug("%s: unable to malloc space for "
	    "MP Floating Pointer Structure\n", __func__);
	return (NULL);
    }
    /* search on 16-byte boundaries */
    for (ofs = 0; ofs < length; ofs += 16) {
	if (!memread(base + ofs, mpfps, sizeof(*mpfps)))
	    break;

	/* compare signature, validate checksum */
	if (!strncmp(mpfps->signature, MPFPS_SIG, strlen(MPFPS_SIG))) {
	    cp = (u_int8_t *)mpfps;
	    sum = 0;
	    /* mpfps is 16 bytes, or one "paragraph" */
	    if (mpfps->length != 1) {
	    	msgDebug("%s: bad mpfps length (%d)\n",
		    __func__, mpfps->length);
		continue;
	    }
	    for (idx = 0; idx < mpfps->length * 16; idx++)
		sum += *(cp + idx);
	    if (sum != 0) {
		msgDebug("%s: bad mpfps checksum (%d)\n", __func__, sum);
		continue;
	    }
	    return (mpfps);
	}
    }
    free(mpfps);
    return (NULL);
}

static mpcth_t
biosmptable_check_mpcth(off_t addr)
{
    mpcth_t mpcth;
    u_int8_t *cp, sum;
    int idx, table_length;

    /* mpcth must be in the first 1MB */
    if ((u_int32_t)addr >= 1024 * 1024) {
	msgDebug("%s: bad mpcth address (0x%lx)\n", __func__, addr);
	return (NULL);
    }

    mpcth = malloc(sizeof(*mpcth));
    if (mpcth == NULL) {
	msgDebug("%s: unable to malloc space for "
	    "MP Configuration Table Header\n", __func__);
	return (NULL);
    }
    if (!memread(addr, mpcth, sizeof(*mpcth)))
	goto bad;
    /* Compare signature and validate checksum. */
    if (strncmp(mpcth->signature, MPCTH_SIG, strlen(MPCTH_SIG)) != 0) {
        msgDebug("%s: bad mpcth signature\n", __func__);
	goto bad;
    }
    table_length = mpcth->base_table_length;
    mpcth = realloc(mpcth, table_length);
    if (mpcth == NULL) {
	msgDebug("%s: unable to realloc space for mpcth (len %u)\n",
	    __func__, table_length);
	return  (NULL);
    }
    if (!memread(addr, mpcth, table_length))
	goto bad;
    cp = (u_int8_t *)mpcth;
    sum = 0;
    for (idx = 0; idx < mpcth->base_table_length; idx++)
	sum += *(cp + idx);
    if (sum != 0) {
	msgDebug("%s: bad mpcth checksum (%d)\n", __func__, sum);
	goto bad;
    }

    return mpcth;
bad:
    free(mpcth);
    return (NULL);
}
