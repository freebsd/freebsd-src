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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/sysctl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <paths.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include <netncp/ncp_lib.h>
#include <netncp/ncp_rcfile.h>
#include <netncp/ncp_nls.h>
/*#include <netncp/ncp_cfg.h>*/
#include <netncp/ncpio.h>

#define	_PATH_NCP	_PATH_DEV NCP_NAME

void
ncp_add_word_lh(struct ncp_buf *conn, u_int16_t x) {
	setwle(conn->packet, conn->rqsize, x);
	conn->rqsize += 2;
	return;
}

void
ncp_add_dword_lh(struct ncp_buf *conn, u_int32_t x) {
	setdle(conn->packet, conn->rqsize, x);
	conn->rqsize += 4;
	return;
}

void
ncp_add_word_hl(struct ncp_buf *conn, u_int16_t x){
	setwbe(conn->packet, conn->rqsize, x);
	conn->rqsize += 2;
	return;
}

void
ncp_add_dword_hl(struct ncp_buf *conn, u_int32_t x) {
	setdbe(conn->packet, conn->rqsize, x);
	conn->rqsize += 4;
	return;
}

void
ncp_add_mem(struct ncp_buf *conn, const void *source, int size) {
	memcpy(conn->packet+conn->rqsize, source, size);
	conn->rqsize += size;
	return;
}

void
ncp_add_mem_nls(struct ncp_buf *conn, const void *source, int size) {
	ncp_nls_mem_u2n(conn->packet+conn->rqsize, source, size);
	conn->rqsize += size;
	return;
}

void
ncp_add_pstring(struct ncp_buf *conn, const char *s) {
	int len = strlen(s);
	if (len > 255) {
		ncp_printf("ncp_add_pstring: string too long: %s\n", s);
		len = 255;
	}
	ncp_add_byte(conn, len);
	ncp_add_mem(conn, s, len);
	return;
}

void
ncp_add_handle_path(struct ncp_buf *conn, nuint32 volNumber, nuint32 dirNumber,
	int handleFlag, const char *path)
{
	ncp_add_byte(conn, volNumber);
	ncp_add_dword_lh(conn, dirNumber);
	ncp_add_byte(conn, handleFlag);
	if (path) {
		ncp_add_byte(conn, 1);		/* 1 component */
		ncp_add_pstring(conn, path);
	} else {
		ncp_add_byte(conn, 0);
	}
}

void
ncp_init_request(struct ncp_buf *conn) {
	conn->rqsize = 0;
	conn->rpsize = 0;
}

void
ncp_init_request_s(struct ncp_buf *conn, int subfn) {
	ncp_init_request(conn);
	ncp_add_word_lh(conn, 0);
	ncp_add_byte(conn, subfn);
}

u_int16_t
ncp_reply_word_hl(struct ncp_buf *conn, int offset) {
	return getwbe(ncp_reply_data(conn, offset), 0);
}

u_int16_t
ncp_reply_word_lh(struct ncp_buf *conn, int offset) {
	return getwle(ncp_reply_data(conn, offset), 0);
}

u_int32_t
ncp_reply_dword_hl(struct ncp_buf *conn, int offset) {
	return getdbe(ncp_reply_data(conn, offset), 0);
}

u_int32_t
ncp_reply_dword_lh(struct ncp_buf *conn, int offset) {
	return getdle(ncp_reply_data(conn, offset), 0);
}


int
ncp_connect(struct ncp_conn_args *li, int *connHandle) {
	struct ncpioc_connect args;
	int fd, r;
	if ((fd = open(_PATH_NCP, O_RDWR)) < 0)
		return (errno);
	args.ioc_li = li;
	args.ioc_connhandle = connHandle;
	errno = 0;
	(void)ioctl(fd, NCPIOC_CONNECT, &args);
	r = errno;
	close(fd);
	return (r);
}

int
ncp_disconnect(int cH) {
	DECLARE_RQ;

	ncp_init_request(conn);
	ncp_add_byte(conn, NCP_CONN_CONNCLOSE);
	return ncp_conn_request(cH, conn);
}

int
ncp_request(int connHandle,int function, struct ncp_buf *ncpbuf){
	struct ncpioc_request args;
	int fd, r;
	if ((fd = open(_PATH_NCP, O_RDWR)) < 0)
		return (errno);
	args.ioc_connhandle = connHandle;
	args.ioc_fn = function;
	args.ioc_ncpbuf = ncpbuf;
	errno = 0;
	(void)ioctl(fd, NCPIOC_REQUEST, &args);
	r = errno;
	close(fd);
	return (r);
}

int
ncp_conn_request(int connHandle, struct ncp_buf *ncpbuf){
	return (ncp_request(connHandle, NCP_CONN, ncpbuf));
}

int
ncp_conn_scan(struct ncp_conn_loginfo *li, int *connid) {
	struct ncpioc_connscan args;
	int fd, r;
	if ((fd = open(_PATH_NCP, O_RDWR)) < 0)
		return (errno);
	args.ioc_li = li;
	args.ioc_connhandle = connid;
	errno = 0;
	(void)ioctl(fd, NCPIOC_CONNSCAN, &args);
	r = errno;
	close(fd);
	return (r);
}

NWCCODE
NWRequest(NWCONN_HANDLE cH, nuint16 fn,
	nuint16 nrq, NW_FRAGMENT* rq, 
	nuint16 nrp, NW_FRAGMENT* rp) 
{
	int error;
	struct ncp_conn_frag nf;
	DECLARE_RQ;

	ncp_init_request(conn);
	ncp_add_byte(conn, NCP_CONN_FRAG);
	nf.fn = fn;
	nf.rqfcnt = nrq;
	nf.rqf = rq;
	nf.rpf = rp;
	nf.rpfcnt = nrp;
	ncp_add_mem(conn, &nf, sizeof(nf));
	error = ncp_conn_request(cH, conn);
	return error;
}


int
ncp_initlib(void){
	int error;
	int kv;
	size_t kvlen = sizeof(kv);
	static int ncp_initialized;

	if (ncp_initialized)
		return 0;
	error = sysctlbyname("net.ncp.version", &kv, &kvlen, NULL, 0);
	if (error) {
		fprintf(stderr, "%s: kernel module is old, please recompile it.\n", __FUNCTION__);
		return error;
	}
	if (NCP_VERSION != kv) {
		fprintf(stderr, "%s: kernel module version(%d) don't match library(%d).\n", __FUNCTION__, kv, NCP_VERSION);
		return EINVAL;
	}
	if ((error = ncp_nls_setrecode(0)) != 0) {
		fprintf(stderr, "%s: can't initialise recode\n", __FUNCTION__);
		return error;
	}
	if ((error = ncp_nls_setlocale("")) != 0) {
		fprintf(stderr, "%s: can't initialise locale\n", __FUNCTION__);
		return error;
	}
	ncp_initialized++;
	return 0;
}


/*
 */
int	ncp_opterr = 1,		/* if error message should be printed */
	ncp_optind = 1,		/* index into parent argv vector */
	ncp_optopt,			/* character checked for validity */
	ncp_optreset;		/* reset getopt */
char	*ncp_optarg;		/* argument associated with option */

#define	BADCH	(int)'?'
#define	BADARG	(int)':'
#define	EMSG	""

int
ncp_getopt(nargc, nargv, ostr)
	int nargc;
	char * const *nargv;
	const char *ostr;
{
	static char *place = EMSG;		/* option letter processing */
	char *oli;				/* option letter list index */
	int tmpind;

	if (ncp_optreset || !*place) {		/* update scanning pointer */
		ncp_optreset = 0;
		tmpind = ncp_optind;
		while (1) {
			if (tmpind >= nargc) {
				place = EMSG;
				return (-1);
			}
			if (*(place = nargv[tmpind]) != '-') {
				tmpind++;
				continue;	/* lookup next option */
			}
			if (place[1] && *++place == '-') {	/* found "--" */
				ncp_optind = ++tmpind;
				place = EMSG;
				return (-1);
			}
			ncp_optind = tmpind;
			break;
		}
	}					/* option letter okay? */
	if ((ncp_optopt = (int)*place++) == (int)':' ||
	    !(oli = strchr(ostr, ncp_optopt))) {
		/*
		 * if the user didn't specify '-' as an option,
		 * assume it means -1.
		 */
		if (ncp_optopt == (int)'-')
			return (-1);
		if (!*place)
			++ncp_optind;
		if (ncp_opterr && *ostr != ':')
			(void)fprintf(stderr,
			    "%s: illegal option -- %c\n", _getprogname(), ncp_optopt);
		return (BADCH);
	}
	if (*++oli != ':') {			/* don't need argument */
		ncp_optarg = NULL;
		if (!*place)
			++ncp_optind;
	}
	else {					/* need an argument */
		if (*place)			/* no white space */
			ncp_optarg = place;
		else if (nargc <= ++ncp_optind) {	/* no arg */
			place = EMSG;
			if (*ostr == ':')
				return (BADARG);
			if (ncp_opterr)
				(void)fprintf(stderr,
				    "%s: option requires an argument -- %c\n",
				    _getprogname(), ncp_optopt);
			return (BADCH);
		}
	 	else				/* white space */
			ncp_optarg = nargv[ncp_optind];
		place = EMSG;
		++ncp_optind;
	}
	return (ncp_optopt);			/* dump back option letter */
}
/*
 * misc options parsing routines
 */
int
ncp_args_parserc(struct ncp_args *na, char *sect, ncp_setopt_t *set_callback) {
	int len, error;

	for (; na->opt; na++) {
		switch (na->at) {
		    case NCA_STR:
			if (rc_getstringptr(ncp_rc,sect,na->name,&na->str) == 0) {
				len = strlen(na->str);
				if (len > na->ival) {
					fprintf(stderr,"rc: Argument for option '%c' (%s) too long\n",na->opt,na->name);
					return EINVAL;
				}
				set_callback(na);
			}
			break;
		    case NCA_BOOL:
			error = rc_getbool(ncp_rc,sect,na->name,&na->ival);
			if (error == ENOENT) break;
			if (error) return EINVAL;
			set_callback(na);
			break;
		    case NCA_INT:
			if (rc_getint(ncp_rc,sect,na->name,&na->ival) == 0) {
				if (((na->flag & NAFL_HAVEMIN) && 
				     (na->ival < na->min)) || 
				    ((na->flag & NAFL_HAVEMAX) && 
				     (na->ival > na->max))) {
					fprintf(stderr,"rc: Argument for option '%c' (%s) should be in [%d-%d] range\n",na->opt,na->name,na->min,na->max);
					return EINVAL;
				}
				set_callback(na);
			};
			break;
		    default:
			break;
		}
	}
	return 0;
}

int
ncp_args_parseopt(struct ncp_args *na, int opt, char *optarg, ncp_setopt_t *set_callback) {
	int len;

	for (; na->opt; na++) {
		if (na->opt != opt) continue;
		switch (na->at) {
		    case NCA_STR:
			na->str = optarg;
			if (optarg) {
				len = strlen(na->str);
				if (len > na->ival) {
					fprintf(stderr,"opt: Argument for option '%c' (%s) too long\n",na->opt,na->name);
					return EINVAL;
				}
				set_callback(na);
			}
			break;
		    case NCA_BOOL:
			na->ival = 0;
			set_callback(na);
			break;
		    case NCA_INT:
			errno = 0;
			na->ival = strtol(optarg, NULL, 0);
			if (errno) {
				fprintf(stderr,"opt: Invalid integer value for option '%c' (%s).\n",na->opt,na->name);
				return EINVAL;
			}
			if (((na->flag & NAFL_HAVEMIN) && 
			     (na->ival < na->min)) || 
			    ((na->flag & NAFL_HAVEMAX) && 
			     (na->ival > na->max))) {
				fprintf(stderr,"opt: Argument for option '%c' (%s) should be in [%d-%d] range\n",na->opt,na->name,na->min,na->max);
				return EINVAL;
			}
			set_callback(na);
			break;
		    default:
			break;
		}
		break;
	}
	return 0;
}

/*
 * Print a (descriptive) error message
 * error values:
 *  	   0 - no specific error code available;
 *  -999..-1 - NDS error
 *  1..32767 - system error
 *  the rest - requester error;
 */
void
ncp_error(const char *fmt, int error, ...) {
	va_list ap;

	fprintf(stderr, "%s: ", _getprogname());
	va_start(ap, error);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	if (error == -1)
		error = errno;
	if (error > -1000 && error < 0) {
		fprintf(stderr, ": dserr = %d\n", error);
	} else if (error & 0x8000) {
		fprintf(stderr, ": nwerr = %04x\n", error);
	} else if (error) {
		fprintf(stderr, ": syserr = %s\n", strerror(error));
	} else
		fprintf(stderr, "\n");
}

char *
ncp_printb(char *dest, int flags, const struct ncp_bitname *bnp) {
	int first = 1;

	strcpy(dest, "<");
	for(; bnp->bn_bit; bnp++) {
		if (flags & bnp->bn_bit) {
			strcat(dest, bnp->bn_name);
			first = 0;
		}
		if (!first && (flags & bnp[1].bn_bit))
			strcat(dest, "|");
	}
	strcat(dest, ">");
	return dest;
}
