/*
 * Copyright (c) 1993, 1994 Jeffrey C. Mogul, Digital Equipment Corporation,
 * Western Research Laboratory. All rights reserved.
 * Copyright (c) 2001 Compaq Computer Corporation. All rights reserved.
 *
 *  Permission to use, copy, and modify this software and its
 *  documentation is hereby granted only under the following terms and
 *  conditions.  Both the above copyright notice and this permission
 *  notice must appear in all copies of the software, derivative works
 *  or modified versions, and any portions thereof, and both notices
 *  must appear in supporting documentation.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *    1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *    2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS" AND COMPAQ COMPUTER CORPORATION
 *  DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
 *  ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.   IN NO
 *  EVENT SHALL COMPAQ COMPUTER CORPORATION BE LIABLE FOR ANY
 *  SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 *  AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 *  OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 *  SOFTWARE.
 */

/*
 * parsenfsfh.c - portable parser for NFS file handles
 *			uses all sorts of heuristics
 *
 * Jeffrey C. Mogul
 * Digital Equipment Corporation
 * Western Research Laboratory
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "netdissect-stdinc.h"

#include <stdio.h>
#include <string.h>

#include "netdissect-ctype.h"

#include "netdissect.h"
#include "extract.h"
#include "nfsfh.h"

/*
 * This routine attempts to parse a file handle (in network byte order),
 * using heuristics to guess what kind of format it is in.  See the
 * file "fhandle_layouts" for a detailed description of the various
 * patterns we know about.
 *
 * The file handle is parsed into our internal representation of a
 * file-system id, and an internal representation of an inode-number.
 */

#define	FHT_UNKNOWN	0
#define	FHT_AUSPEX	1
#define	FHT_DECOSF	2
#define	FHT_IRIX4	3
#define	FHT_IRIX5	4
#define	FHT_SUNOS3	5
#define	FHT_SUNOS4	6
#define	FHT_ULTRIX	7
#define	FHT_VMSUCX	8
#define	FHT_SUNOS5	9
#define	FHT_AIX32	10
#define	FHT_HPUX9	11
#define	FHT_BSD44	12

static int is_UCX(netdissect_options *, const unsigned char *, u_int);

void
Parse_fh(netdissect_options *ndo, const unsigned char *fh, u_int len,
	 my_fsid *fsidp, uint32_t *inop,
	 const char **osnamep, /* if non-NULL, return OS name here */
	 const char **fsnamep, /* if non-NULL, return server fs name here (for VMS) */
	 int ourself)	/* true if file handle was generated on this host */
{
	const unsigned char *fhp = fh;
	uint32_t temp;
	int fhtype = FHT_UNKNOWN;
	u_int i;

	/*
	 * Require at least 16 bytes of file handle; it's variable-length
	 * in NFSv3.  "len" is in units of 32-bit words, not bytes.
	 */
	if (len < 16/4)
		fhtype = FHT_UNKNOWN;
	else {
		if (ourself) {
		    /* File handle generated on this host, no need for guessing */
#if	defined(IRIX40)
		    fhtype = FHT_IRIX4;
#endif
#if	defined(IRIX50)
		    fhtype = FHT_IRIX5;
#endif
#if	defined(IRIX51)
		    fhtype = FHT_IRIX5;
#endif
#if	defined(SUNOS4)
		    fhtype = FHT_SUNOS4;
#endif
#if	defined(SUNOS5)
		    fhtype = FHT_SUNOS5;
#endif
#if	defined(ultrix)
		    fhtype = FHT_ULTRIX;
#endif
#if	defined(__osf__)
		    fhtype = FHT_DECOSF;
#endif
#if	defined(__NetBSD__) || defined(__FreeBSD__) || defined(__DragonFly__) \
     || defined(__OpenBSD__)
		    fhtype = FHT_BSD44;
#endif
		}
		/*
		 * This is basically a big decision tree
		 */
		else if ((GET_U_1(fhp) == 0) && (GET_U_1(fhp + 1) == 0)) {
		    /* bytes[0,1] == (0,0); rules out Ultrix, IRIX5, SUNOS5 */
		    /* probably rules out HP-UX, AIX unless they allow major=0 */
		    if ((GET_U_1(fhp + 2) == 0) && (GET_U_1(fhp + 3) == 0)) {
			/* bytes[2,3] == (0,0); must be Auspex */
			/* XXX or could be Ultrix+MASSBUS "hp" disk? */
			fhtype = FHT_AUSPEX;
		    }
		    else {
			/*
			 * bytes[2,3] != (0,0); rules out Auspex, could be
			 * DECOSF, SUNOS4, or IRIX4
			 */
			if ((GET_U_1(fhp + 4) != 0) && (GET_U_1(fhp + 5) == 0) &&
				(GET_U_1(fhp + 8) == 12) && (GET_U_1(fhp + 9) == 0)) {
			    /* seems to be DECOSF, with minor == 0 */
			    fhtype = FHT_DECOSF;
			}
			else {
			    /* could be SUNOS4 or IRIX4 */
			    /* XXX the test of fhp[5] == 8 could be wrong */
			    if ((GET_U_1(fhp + 4) == 0) && (GET_U_1(fhp + 5) == 8) && (GET_U_1(fhp + 6) == 0) &&
			        (GET_U_1(fhp + 7) == 0)) {
				/* looks like a length, not a file system typecode */
				fhtype = FHT_IRIX4;
			    }
			    else {
				/* by elimination */
				fhtype = FHT_SUNOS4;
			    }
			}
		    }
		}
		else {
		    /*
		     * bytes[0,1] != (0,0); rules out Auspex, IRIX4, SUNOS4
		     * could be IRIX5, DECOSF, UCX, Ultrix, SUNOS5
		     * could be AIX, HP-UX
		     */
		    if ((GET_U_1(fhp + 2) == 0) && (GET_U_1(fhp + 3) == 0)) {
			/*
			 * bytes[2,3] == (0,0); rules out OSF, probably not UCX
			 * (unless the exported device name is just one letter!),
			 * could be Ultrix, IRIX5, AIX, or SUNOS5
			 * might be HP-UX (depends on their values for minor devs)
			 */
			if ((GET_U_1(fhp + 6) == 0) && (GET_U_1(fhp + 7) == 0)) {
			    fhtype = FHT_BSD44;
			}
			/*XXX we probably only need to test of these two bytes */
			else if ((len >= 24/4) && (GET_U_1(fhp + 21) == 0) && (GET_U_1(fhp + 23) == 0)) {
			    fhtype = FHT_ULTRIX;
			}
			else {
			    /* Could be SUNOS5/IRIX5, maybe AIX */
			    /* XXX no obvious difference between SUNOS5 and IRIX5 */
			    if (GET_U_1(fhp + 9) == 10)
				fhtype = FHT_SUNOS5;
			    /* XXX what about AIX? */
			}
		    }
		    else {
			/*
			 * bytes[2,3] != (0,0); rules out Ultrix, could be
			 * DECOSF, SUNOS5, IRIX5, AIX, HP-UX, or UCX
			 */
			if ((GET_U_1(fhp + 8) == 12) && (GET_U_1(fhp + 9) == 0)) {
			    fhtype = FHT_DECOSF;
			}
			else if ((GET_U_1(fhp + 8) == 0) && (GET_U_1(fhp + 9) == 10)) {
			    /* could be SUNOS5/IRIX5, AIX, HP-UX */
			    if ((GET_U_1(fhp + 7) == 0) && (GET_U_1(fhp + 6) == 0) &&
				(GET_U_1(fhp + 5) == 0) && (GET_U_1(fhp + 4) == 0)) {
				/* XXX is this always true of HP-UX? */
				fhtype = FHT_HPUX9;
			    }
			    else if (GET_U_1(fhp + 7) == 2) {
				/* This would be MNT_NFS on AIX, which is impossible */
				fhtype = FHT_SUNOS5;	/* or maybe IRIX5 */
			    }
			    else {
				/*
				 * XXX Could be SUNOS5/IRIX5 or AIX.  I don't
				 * XXX see any way to disambiguate these, so
				 * XXX I'm going with the more likely guess.
				 * XXX Sorry, Big Blue.
				 */
				fhtype = FHT_SUNOS5;	/* or maybe IRIX5 */
			    }
		        }
			else {
			    if (is_UCX(ndo, fhp, len)) {
				fhtype = FHT_VMSUCX;
			    }
			    else {
				fhtype = FHT_UNKNOWN;
			    }
			}
		    }
		}
	}

	/* XXX still needs to handle SUNOS3 */

	switch (fhtype) {
	case FHT_AUSPEX:
	    fsidp->Fsid_dev.Minor = GET_U_1(fhp + 7);
	    fsidp->Fsid_dev.Major = GET_U_1(fhp + 6);
	    fsidp->fsid_code = 0;

	    *inop = GET_BE_U_4(fhp + 12);

	    if (osnamep)
		*osnamep = "Auspex";
	    break;

	case FHT_BSD44:
	    fsidp->Fsid_dev.Minor = GET_U_1(fhp);
	    fsidp->Fsid_dev.Major = GET_U_1(fhp + 1);
	    fsidp->fsid_code = 0;

	    *inop = GET_LE_U_4(fhp + 12);

	    if (osnamep)
		*osnamep = "BSD 4.4";
	    break;

	case FHT_DECOSF:
	    fsidp->fsid_code = GET_LE_U_4(fhp + 4);
			/* XXX could ignore 3 high-order bytes */

	    temp = GET_LE_U_4(fhp);
	    fsidp->Fsid_dev.Minor = temp & 0xFFFFF;
	    fsidp->Fsid_dev.Major = (temp>>20) & 0xFFF;

	    *inop = GET_LE_U_4(fhp + 12);
	    if (osnamep)
		*osnamep = "OSF";
	    break;

	case FHT_IRIX4:
	    fsidp->Fsid_dev.Minor = GET_U_1(fhp + 3);
	    fsidp->Fsid_dev.Major = GET_U_1(fhp + 2);
	    fsidp->fsid_code = 0;

	    *inop = GET_BE_U_4(fhp + 8);

	    if (osnamep)
		*osnamep = "IRIX4";
	    break;

	case FHT_IRIX5:
	    fsidp->Fsid_dev.Minor = GET_BE_U_2(fhp + 2);
	    fsidp->Fsid_dev.Major = GET_BE_U_2(fhp);
	    fsidp->fsid_code = GET_BE_U_4(fhp + 4);

	    *inop = GET_BE_U_4(fhp + 12);

	    if (osnamep)
		*osnamep = "IRIX5";
	    break;

#ifdef notdef
	case FHT_SUNOS3:
	    /*
	     * XXX - none of the heuristics above return this.
	     * Are there any SunOS 3.x systems around to care about?
	     */
	    if (osnamep)
		*osnamep = "SUNOS3";
	    break;
#endif

	case FHT_SUNOS4:
	    fsidp->Fsid_dev.Minor = GET_U_1(fhp + 3);
	    fsidp->Fsid_dev.Major = GET_U_1(fhp + 2);
	    fsidp->fsid_code = GET_BE_U_4(fhp + 4);

	    *inop = GET_BE_U_4(fhp + 12);

	    if (osnamep)
		*osnamep = "SUNOS4";
	    break;

	case FHT_SUNOS5:
	    temp = GET_BE_U_2(fhp);
	    fsidp->Fsid_dev.Major = (temp>>2) &  0x3FFF;
	    temp = GET_BE_U_3(fhp + 1);
	    fsidp->Fsid_dev.Minor = temp & 0x3FFFF;
	    fsidp->fsid_code = GET_BE_U_4(fhp + 4);

	    *inop = GET_BE_U_4(fhp + 12);

	    if (osnamep)
		*osnamep = "SUNOS5";
	    break;

	case FHT_ULTRIX:
	    fsidp->fsid_code = 0;
	    fsidp->Fsid_dev.Minor = GET_U_1(fhp);
	    fsidp->Fsid_dev.Major = GET_U_1(fhp + 1);

	    temp = GET_LE_U_4(fhp + 4);
	    *inop = temp;
	    if (osnamep)
		*osnamep = "Ultrix";
	    break;

	case FHT_VMSUCX:
	    /* No numeric file system ID, so hash on the device-name */
	    if (sizeof(*fsidp) >= 14) {
		if (sizeof(*fsidp) > 14)
		    memset((char *)fsidp, 0, sizeof(*fsidp));
		/* just use the whole thing */
		memcpy((char *)fsidp, (const char *)fh, 14);
	    }
	    else {
		uint32_t tempa[4];	/* at least 16 bytes, maybe more */

		memset((char *)tempa, 0, sizeof(tempa));
		memcpy((char *)tempa, (const char *)fh, 14); /* ensure alignment */
		fsidp->Fsid_dev.Minor = tempa[0] + (tempa[1]<<1);
		fsidp->Fsid_dev.Major = tempa[2] + (tempa[3]<<1);
		fsidp->fsid_code = 0;
	    }

	    /* VMS file ID is: (RVN, FidHi, FidLo) */
	    *inop = (((uint32_t) GET_U_1(fhp + 26)) << 24) |
		    (((uint32_t) GET_U_1(fhp + 27)) << 16) |
		    (GET_LE_U_2(fhp + 22) << 0);

	    /* Caller must save (and null-terminate?) this value */
	    if (fsnamep)
		*fsnamep = (const char *)(fhp + 1);

	    if (osnamep)
		*osnamep = "VMS";
	    break;

	case FHT_AIX32:
	    fsidp->Fsid_dev.Minor = GET_BE_U_2(fhp + 2);
	    fsidp->Fsid_dev.Major = GET_BE_U_2(fhp);
	    fsidp->fsid_code = GET_BE_U_4(fhp + 4);

	    *inop = GET_BE_U_4(fhp + 12);

	    if (osnamep)
		*osnamep = "AIX32";
	    break;

	case FHT_HPUX9:
	    fsidp->Fsid_dev.Major = GET_U_1(fhp);
	    temp = GET_BE_U_3(fhp + 1);
	    fsidp->Fsid_dev.Minor = temp;
	    fsidp->fsid_code = GET_BE_U_4(fhp + 4);

	    *inop = GET_BE_U_4(fhp + 12);

	    if (osnamep)
		*osnamep = "HPUX9";
	    break;

	case FHT_UNKNOWN:
#ifdef DEBUG
	    /* XXX debugging */
	    for (i = 0; i < len*4; i++)
		(void)fprintf(stderr, "%x.", GET_U_1(fhp + i));
	    (void)fprintf(stderr, "\n");
#endif
	    /* Save the actual handle, so it can be display with -u */
	    for (i = 0; i < len*4 && i*2 < sizeof(fsidp->Opaque_Handle) - 1; i++)
		(void)snprintf(&(fsidp->Opaque_Handle[i*2]), 3, "%.2X",
			       GET_U_1(fhp + i));
	    fsidp->Opaque_Handle[i*2] = '\0';

	    /* XXX for now, give "bogus" values to aid debugging */
	    fsidp->fsid_code = 0;
	    fsidp->Fsid_dev.Minor = 257;
	    fsidp->Fsid_dev.Major = 257;
	    *inop = 1;

	    /* display will show this string instead of (257,257) */
	    if (fsnamep)
		*fsnamep = "Unknown";

	    if (osnamep)
		*osnamep = "Unknown";
	    break;

	}
}

/*
 * Is this a VMS UCX file handle?
 *	Check for:
 *	(1) leading code byte	[XXX not yet]
 *	(2) followed by string of printing chars & spaces
 *	(3) followed by string of nulls
 */
static int
is_UCX(netdissect_options *ndo, const unsigned char *fhp, u_int len)
{
	u_int i;
	int seen_null = 0;

	/*
	 * Require at least 28 bytes of file handle; it's variable-length
	 * in NFSv3.  "len" is in units of 32-bit words, not bytes.
	 */
	if (len < 28/4)
		return(0);

	for (i = 1; i < 14; i++) {
	    if (ND_ASCII_ISPRINT(GET_U_1(fhp + i))) {
		if (seen_null)
		   return(0);
		else
		   continue;
	    }
	    else if (GET_U_1(fhp + i) == 0) {
		seen_null = 1;
		continue;
	    }
	    else
		return(0);
	}

	return(1);
}
