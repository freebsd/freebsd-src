/*
 * Copyright (c) 2000-2001, Boris Popov
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
 * $FreeBSD: src/sys/sys/iconv.h,v 1.5 2002/07/15 13:34:50 markm Exp $
 */
#ifndef _SYS_ICONV_H_
#define _SYS_ICONV_H_

#define	ICONV_CSNMAXLEN		31	/* maximum length of charset name */
#define	ICONV_CNVNMAXLEN	31	/* maximum length of converter name */
#define	ICONV_CSMAXDATALEN	1024	/* maximum size of data associated with cs pair */

/*
 * Entry for cslist sysctl
 */
#define	ICONV_CSPAIR_INFO_VER	1

struct iconv_cspair_info {
	int	cs_version;
	int	cs_id;
	int	cs_base;
	int	cs_refcount;
	char	cs_to[ICONV_CSNMAXLEN];
	char	cs_from[ICONV_CSNMAXLEN];
};

/*
 * Paramters for 'add' sysctl
 */
#define	ICONV_ADD_VER	1

struct iconv_add_in {
	int	ia_version;
	char	ia_converter[ICONV_CNVNMAXLEN];
	char	ia_to[ICONV_CSNMAXLEN];
	char	ia_from[ICONV_CSNMAXLEN];
	int	ia_datalen;
	const void *ia_data;
};

struct iconv_add_out {
	int	ia_csid;
};

#ifndef _KERNEL

__BEGIN_DECLS

int   kiconv_add_xlat_table(const char *, const char *, const u_char *);

__END_DECLS

#else /* !_KERNEL */

#include <sys/kobj.h>
#include <sys/queue.h>			/* can't avoid that */
#include <sys/sysctl.h>			/* can't avoid that */

struct iconv_cspair;
struct iconv_cspairdata;

/*
 * iconv converter class definition
 */
struct iconv_converter_class {
	KOBJ_CLASS_FIELDS;
	TAILQ_ENTRY(iconv_converter_class)	cc_link;
};

struct iconv_cspair {
	int		cp_id;		/* unique id of charset pair */
	int		cp_refcount;	/* number of references from other pairs */
	const char *	cp_from;
	const char *	cp_to;
	void *		cp_data;
	struct iconv_converter_class * cp_dcp;
	struct iconv_cspair *cp_base;
	TAILQ_ENTRY(iconv_cspair)	cp_link;
};

#define	KICONV_CONVERTER(name,size) 				\
    static DEFINE_CLASS(iconv_ ## name, iconv_ ## name ## _methods, (size)); \
    static moduledata_t iconv_ ## name ## _mod = {	\
	"iconv_"#name, iconv_converter_handler,		\
	(void*)&iconv_ ## name ## _class		\
    };							\
    DECLARE_MODULE(iconv_ ## name, iconv_ ## name ## _mod, SI_SUB_DRIVERS, SI_ORDER_ANY);

#define	KICONV_CES(name,size) 				\
    static DEFINE_CLASS(iconv_ces_ ## name, iconv_ces_ ## name ## _methods, (size)); \
    static moduledata_t iconv_ces_ ## name ## _mod = {	\
	"iconv_ces_"#name, iconv_cesmod_handler,	\
	(void*)&iconv_ces_ ## name ## _class		\
    };							\
    DECLARE_MODULE(iconv_ces_ ## name, iconv_ces_ ## name ## _mod, SI_SUB_DRIVERS, SI_ORDER_ANY);

#ifdef MALLOC_DECLARE
MALLOC_DECLARE(M_ICONV);
#endif

/*
 * Basic conversion functions
 */
int iconv_open(const char *to, const char *from, void **handle);
int iconv_close(void *handle);
int iconv_conv(void *handle, const char **inbuf,
	size_t *inbytesleft, char **outbuf, size_t *outbytesleft);
char* iconv_convstr(void *handle, char *dst, const char *src);
void* iconv_convmem(void *handle, void *dst, const void *src, int size);

/*
 * Internal functions
 */
int iconv_lookupcp(char **cpp, const char *s);

int iconv_converter_initstub(struct iconv_converter_class *dp);
int iconv_converter_donestub(struct iconv_converter_class *dp);
int iconv_converter_handler(module_t mod, int type, void *data);

#ifdef ICONV_DEBUG
#define ICDEBUG(format, ...) printf("%s: "format, __func__ , __VA_ARGS__)
#else
#define ICDEBUG(format, ...)
#endif

#endif /* !_KERNEL */

#endif /* !_SYS_ICONV_H_ */
