/*
 *
 *	@doc RESOLVE
 *
 * @module res_comp.c |
 *
 *	  Contains the implementations for dn_comp and rdn_expand as well as
 *	  some other functions used internally by these two functions.
 *
 *	  WSHelper DNS/Hesiod Library for WINSOCK
 *
 */

/*
 * Copyright (c) 1985 Regents of the University of California.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
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

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)res_comp.c	6.22 (Berkeley) 3/19/91";
#endif /* LIBC_SCCS and not lint */

#include <windows.h>
#include <winsock.h>
#include <resolv.h>
#include <stdio.h>


static dn_find();

/*
	replacement for dn_expand called rdn_expand. Older versions of
	the DLL used to this as dn_expand but this has caused some
	conflict with more recent versions of the MSDEV
	libraries. rdn_expand() expands the compressed domain name comp_dn to
	a full domain name.  Expanded names are converted to upper case.

	\param[in]	msg		msg is a pointer to the  beginning  of  the  message
	\param[in]	eomorig
	\param[in]			comp_dn the compressed domain name.
	\param[in, out]		expn_dn	a pointer to the result buffer
	\param[in]			length	size of the result in expn_dn

	\retval				the size of compressed name is returned or -1 if there was an error.
*/



int WINAPI
rdn_expand(const u_char *msg, const u_char *eomorig,
		   const u_char *comp_dn, u_char *exp_dn, int length)
{
    register u_char *cp, *dn;
    register int n, c;
    u_char *eom;
	INT_PTR len = -1;
    int checked = 0;

    dn = exp_dn;
    cp = (u_char *)comp_dn;
    eom = exp_dn + length;
    /*
     * fetch next label in domain name
     */
    while (n = *cp++) {
        /*
         * Check for indirection
         */
        switch (n & INDIR_MASK) {
        case 0:
            if (dn != exp_dn) {
                if (dn >= eom)
                    return (-1);
                *dn++ = '.';
            }
            if (dn+n >= eom)
                return (-1);
            checked += n + 1;
            while (--n >= 0) {
                if ((c = *cp++) == '.') {
                    if (dn + n + 2 >= eom)
                        return (-1);
                    *dn++ = '\\';
                }
                *dn++ = c;
                if (cp >= eomorig)      /* out of range */
                    return(-1);
            }
            break;

        case INDIR_MASK:
            if (len < 0)
                len = cp - comp_dn + 1;
            cp = (u_char *)msg + (((n & 0x3f) << 8) | (*cp & 0xff));
            if (cp < msg || cp >= eomorig)  /* out of range */
                return(-1);
            checked += 2;
            /*
             * Check for loops in the compressed name;
             * if we've looked at the whole message,
             * there must be a loop.
             */
            if (checked >= eomorig - msg)
                return (-1);
            break;

        default:
            return (-1);                    /* flag error */
        }
    }
    *dn = '\0';
    if (len < 0)
        len = cp - comp_dn;
    return (int)(len);
}


/*
	Compress domain name 'exp_dn' into 'comp_dn'
	\param[in]	exp_dn	name to compress
	\param[in, out]	comp_dn		result of the compression
	\paramp[in]	length			the size of the array pointed to by 'comp_dn'.
	\param[in, out]	dnptrs		a list of pointers to previous compressed names. dnptrs[0]
								is a pointer to the beginning of the message. The list ends with NULL.
	\param[in]	lastdnptr		a pointer to the end of the arrary pointed to by 'dnptrs'. Side effect
								is to update the list of pointers for labels inserted into the
								message as we compress the name. If 'dnptr' is NULL, we don't try to
								compress names. If 'lastdnptr' is NULL, we don't update the list.
	\retval						Return the size of the compressed name or -1
 */
int WINAPI
dn_comp(const u_char *exp_dn, u_char *comp_dn, int length,
        u_char **dnptrs, u_char **lastdnptr)
{
    register u_char *cp, *dn;
    register int c, l;
    u_char **cpp, **lpp, *sp, *eob;
    u_char *msg;

    dn = (u_char *)exp_dn;
    cp = comp_dn;
    eob = cp + length;
    if (dnptrs != NULL) {
        if ((msg = *dnptrs++) != NULL) {
            for (cpp = dnptrs; *cpp != NULL; cpp++)
                ;
            lpp = cpp;      /* end of list to search */
        }
    } else
        msg = NULL;
    for (c = *dn++; c != '\0'; ) {
        /* look to see if we can use pointers */
        if (msg != NULL) {
            if ((l = dn_find(dn-1, msg, dnptrs, lpp)) >= 0) {
                if (cp+1 >= eob)
                    return (-1);
                *cp++ = (l >> 8) | INDIR_MASK;
                *cp++ = l % 256;
                return (int)(cp - comp_dn);
            }
            /* not found, save it */
            if (lastdnptr != NULL && cpp < lastdnptr-1) {
                *cpp++ = cp;
                *cpp = NULL;
            }
        }
        sp = cp++;      /* save ptr to length byte */
        do {
            if (c == '.') {
                c = *dn++;
                break;
            }
            if (c == '\\') {
                if ((c = *dn++) == '\0')
                    break;
            }
            if (cp >= eob) {
                if (msg != NULL)
                    *lpp = NULL;
                return (-1);
            }
            *cp++ = c;
        } while ((c = *dn++) != '\0');
        /* catch trailing '.'s but not '..' */
        if ((l =(int)( cp - sp - 1)) == 0 && c == '\0') {
            cp--;
            break;
        }
        if (l <= 0 || l > MAXLABEL) {
            if (msg != NULL)
                *lpp = NULL;
            return (-1);
        }
        *sp = l;
    }
    if (cp >= eob) {
        if (msg != NULL)
            *lpp = NULL;
        return (-1);
    }
    *cp++ = '\0';
    return (int)(cp - comp_dn);
}

/*
 * Skip over a compressed domain name. Return the size or -1.
 */
__dn_skipname(const u_char *comp_dn, const u_char *eom)
{
    register u_char *cp;
    register int n;

    cp = (u_char *)comp_dn;
    while (cp < eom && (n = *cp++)) {
        /*
         * check for indirection
         */
        switch (n & INDIR_MASK) {
        case 0:         /* normal case, n == len */
            cp += n;
            continue;
        default:        /* illegal type */
            return (-1);
        case INDIR_MASK:        /* indirection */
            cp++;
        }
        break;
    }
    return (int)(cp - comp_dn);
}

/*
 * Search for expanded name from a list of previously compressed names.
 * Return the offset from msg if found or -1.
 * dnptrs is the pointer to the first name on the list,
 * not the pointer to the start of the message.
 */
static
dn_find(u_char *exp_dn, u_char *msg, u_char **dnptrs, u_char **lastdnptr)
{
    register u_char *dn, *cp, **cpp;
    register int n;
    u_char *sp;

    for (cpp = dnptrs; cpp < lastdnptr; cpp++) {
        dn = exp_dn;
        sp = cp = *cpp;
        while (n = *cp++) {
            /*
             * check for indirection
             */
            switch (n & INDIR_MASK) {
            case 0:         /* normal case, n == len */
                while (--n >= 0) {
                    if (*dn == '.')
                        goto next;
                    if (*dn == '\\')
                        dn++;
                    if (*dn++ != *cp++)
                        goto next;
                }
                if ((n = *dn++) == '\0' && *cp == '\0')
                    return (int)(sp - msg);
                if (n == '.')
                    continue;
                goto next;

            default:        /* illegal type */
                return (-1);

            case INDIR_MASK:        /* indirection */
                cp = msg + (((n & 0x3f) << 8) | *cp);
            }
        }
        if (*dn == '\0')
            return (int)(sp - msg);
    next:   ;
    }
    return (-1);
}

/*
 * Routines to insert/extract short/long's. Must account for byte
 * order and non-alignment problems. This code at least has the
 * advantage of being portable.
 *
 * used by sendmail.
 */

u_short
_getshort(u_char *msgp)
{
    register u_char *p = (u_char *) msgp;
#ifdef vax
    /*
     * vax compiler doesn't put shorts in registers
     */
    register u_long u;
#else
    register u_short u;
#endif

    u = *p++ << 8;
    return ((u_short)(u | *p));
}

u_long
_getlong(u_char *msgp)
{
    register u_char *p = (u_char *) msgp;
    register u_long u;

    u = *p++; u <<= 8;
    u |= *p++; u <<= 8;
    u |= *p++; u <<= 8;
    return (u | *p);
}

void
__putshort(register u_short s, register u_char *msgp)
{
    msgp[1] = LOBYTE(s);
    msgp[0] = HIBYTE(s);
}

void
__putlong(register u_long l, register u_char *msgp)
{
    msgp[3] = LOBYTE(LOWORD(l));
    msgp[2] = HIBYTE(LOWORD(l));
    msgp[1] = LOBYTE(HIWORD(l));
    msgp[0] = HIBYTE(HIWORD(l));
}
