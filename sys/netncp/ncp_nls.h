/*
 * Copyright (c) 1999, Boris Popov
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
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
#ifndef _NETNCP_NCP_NLS_H_
#define _NETNCP_NCP_NLS_H_

/* options for handle path & caseopt in mount struct */
#define NWHP_HDB	0x01	/* have dir base */
#define	NWHP_UPPER	0x02	/* local names has upper case */
#define	NWHP_LOWER	0x04	/* --"-- lower case */
#define	NWHP_DOS	0x08	/* using dos name space */
#define	NWHP_NLS	0x10	/* do name translation via tables, any nmspc */
#define	NWHP_NOSTRICT	0x20	/* pretend to be a case insensitive */

struct ncp_nlstables {
	u_char	*to_lower;	/* local charset to lower case */
	u_char	*to_upper;	/* local charset to upper case */
	u_char	*n2u;		/* NetWare to Unix */
	u_char	*u2n;
	int	opt;		/* may depend on context */
};

#ifndef _KERNEL
/*
 * NLS, supported character conversion schemes.
 * NCP_NLS_UNIXCHARSET_NETWARECHARSET
 */
#define	NCP_NLS_AS_IS		1
#define	NCP_NLS_AS_IS_NAME	"asis"
#define	NCP_NLS_KOI_866		2
#define	NCP_NLS_SE		3
#define	NCP_NLS_KOI_866_NAME	"koi2cp866"
#define	NCP_NLS_SE_NAME		"se"

extern struct ncp_nlstables ncp_nls;	/* active nls */

__BEGIN_DECLS

int   ncp_nls_setrecode(int scheme);
int   ncp_nls_setrecodebyname(char *name);
int   ncp_nls_setlocale(char *name);
char* ncp_nls_str_n2u(char *dst, const char *src);
char* ncp_nls_str_u2n(char *dst, const char *src);
char* ncp_nls_mem_n2u(char *dst, const char *src, int size);
char* ncp_nls_mem_u2n(char *dst, const char *src, int size);

__END_DECLS

#else /* !_KERNEL */


extern struct ncp_nlstables ncp_defnls;

void ncp_str_upper(char *name);
void ncp_str_lower(char *name);
void ncp_pathcopy(char *src, char *dst, int len, struct ncp_nlstables *nt);
int  ncp_pathcheck(char *s, int len, struct ncp_nlstables *nt, int strict);
void ncp_path2unix(char *src, char *dst, int len, struct ncp_nlstables *nt);

#endif /* !_KERNEL */

#endif /* _NCP_NCP_NLS_H_ */
