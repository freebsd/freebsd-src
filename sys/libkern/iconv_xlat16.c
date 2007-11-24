/*-
 * Copyright (c) 2003, Ryuichiro Imura
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/iconv.h>

#include "iconv_converter_if.h"

/*
 * "XLAT16" converter
 */

#ifdef MODULE_DEPEND
MODULE_DEPEND(iconv_xlat16, libiconv, 2, 2, 2);
#endif

/*
 * XLAT16 converter instance
 */
struct iconv_xlat16 {
	KOBJ_FIELDS;
	uint32_t *		d_table[0x200];
	struct iconv_cspair *	d_csp;
};

static int
iconv_xlat16_open(struct iconv_converter_class *dcp,
	struct iconv_cspair *csp, struct iconv_cspair *cspf, void **dpp)
{
	struct iconv_xlat16 *dp;
	uint32_t *headp, **idxp;
	int i;

	dp = (struct iconv_xlat16 *)kobj_create((struct kobj_class*)dcp, M_ICONV, M_WAITOK);
	headp = (uint32_t *)((caddr_t)csp->cp_data + sizeof(dp->d_table));
	idxp = (uint32_t **)csp->cp_data;
	for (i = 0 ; i < 0x200 ; i++) {
		if (*idxp) {
			dp->d_table[i] = headp;
			headp += 0x80;
		} else {
			dp->d_table[i] = NULL;
		}
		idxp++;
	}
	dp->d_csp = csp;
	csp->cp_refcount++;
	*dpp = (void*)dp;
	return (0);
}

static int
iconv_xlat16_close(void *data)
{
	struct iconv_xlat16 *dp = data;

	dp->d_csp->cp_refcount--;
	kobj_delete((struct kobj*)data, M_ICONV);
	return (0);
}

static int
iconv_xlat16_conv(void *d2p, const char **inbuf,
	size_t *inbytesleft, char **outbuf, size_t *outbytesleft,
	int convchar, int casetype)
{
	struct iconv_xlat16 *dp = (struct iconv_xlat16*)d2p;
	const char *src;
	char *dst;
	int nullin, ret = 0;
	size_t in, on, ir, or, inlen;
	uint32_t code;
	u_char u, l;
	uint16_t c1, c2;

	if (inbuf == NULL || *inbuf == NULL || outbuf == NULL || *outbuf == NULL)
		return (0);
	ir = in = *inbytesleft;
	or = on = *outbytesleft;
	src = *inbuf;
	dst = *outbuf;

	while(ir > 0 && or > 0) {

		inlen = 0;
		code = '\0';

		c1 = ir > 1 ? *(src+1) & 0xff : 0;
		c2 = *src & 0xff;

		c1 = c2 & 0x80 ? c1 | 0x100 : c1;
		c2 = c2 & 0x80 ? c2 & 0x7f : c2;

		if (ir > 1 && dp->d_table[c1]) {
			/*
			 * inbuf char is a double byte char
			 */
			code = dp->d_table[c1][c2];
			if (code)
				inlen = 2;
		}

		if (inlen == 0) {
			c1 &= 0xff00;
			if (!dp->d_table[c1]) {
				ret = -1;
				break;
			}
			/*
			 * inbuf char is a single byte char
			 */
			inlen = 1;
			code = dp->d_table[c1][c2];
			if (!code) {
				ret = -1;
				break;
			}
		}

		nullin = (code & XLAT16_ACCEPT_NULL_IN) ? 1 : 0;
		if (inlen == 1 && nullin) {
			/*
			 * XLAT16_ACCEPT_NULL_IN requires inbuf has 2byte
			 */
			ret = -1;
			break;
		}

		/*
		 * now start translation
		 */
		if ((casetype == KICONV_FROM_LOWER && code & XLAT16_HAS_FROM_LOWER_CASE) ||
		    (casetype == KICONV_FROM_UPPER && code & XLAT16_HAS_FROM_UPPER_CASE)) {
			c2 = (u_char)(code >> 16);
			c1 = c2 & 0x80 ? 0x100 : 0;
			c2 = c2 & 0x80 ? c2 & 0x7f : c2;
			code = dp->d_table[c1][c2];
		}

		u = (u_char)(code >> 8);
		l = (u_char)code;

#ifdef XLAT16_ACCEPT_3BYTE_CHR
		if (code & XLAT16_IS_3BYTE_CHR) {
			if (or < 3) {
				ret = -1;
				break;
			}
			*dst++ = u;
			*dst++ = l;
			*dst++ = (u_char)(code >> 16);
			or -= 3;
		} else
#endif
		if (u || code & XLAT16_ACCEPT_NULL_OUT) {
			if (or < 2) {
				ret = -1;
				break;
			}
			*dst++ = u;
			*dst++ = l;
			or -= 2;
		} else {
			if ((casetype == KICONV_LOWER && code & XLAT16_HAS_LOWER_CASE) ||
			    (casetype == KICONV_UPPER && code & XLAT16_HAS_UPPER_CASE))
				*dst++ = (u_char)(code >> 16);
			else
				*dst++ = l;
			or--;
		}

		if (inlen == 2) {
			/*
			 * there is a case that inbuf char is a single
			 * byte char while inlen == 2
			 */
			if ((u_char)*(src+1) == 0 && !nullin ) {
				src++;
				ir--;
			} else {
				src += 2;
				ir -= 2;
			}
		} else {
			src++;
			ir--;
		}

		if (convchar == 1)
			break;
	}

	*inbuf += in - ir;
	*outbuf += on - or;
	*inbytesleft -= in - ir;
	*outbytesleft -= on - or;
	return (ret);
}

static const char *
iconv_xlat16_name(struct iconv_converter_class *dcp)
{
	return ("xlat16");
}

static kobj_method_t iconv_xlat16_methods[] = {
	KOBJMETHOD(iconv_converter_open,	iconv_xlat16_open),
	KOBJMETHOD(iconv_converter_close,	iconv_xlat16_close),
	KOBJMETHOD(iconv_converter_conv,	iconv_xlat16_conv),
#if 0
	KOBJMETHOD(iconv_converter_init,	iconv_xlat16_init),
	KOBJMETHOD(iconv_converter_done,	iconv_xlat16_done),
#endif
	KOBJMETHOD(iconv_converter_name,	iconv_xlat16_name),
	{0, 0}
};

KICONV_CONVERTER(xlat16, sizeof(struct iconv_xlat16));
