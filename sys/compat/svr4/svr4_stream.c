/*-
 * Copyright (c) 1998 Mark Newton.  All rights reserved.
 * Copyright (c) 1994, 1996 Christos Zoulas.  All rights reserved.
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
 *	This product includes software developed by Christos Zoulas.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Pretend that we have streams...
 * Yes, this is gross.
 *
 * ToDo: The state machine for getmsg needs re-thinking
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_compat.h"
#include "opt_ktrace.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/capability.h>
#include <sys/fcntl.h>
#include <sys/filedesc.h>
#include <sys/filio.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/file.h> 		/* Must come after sys/malloc.h */
#include <sys/mbuf.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/signal.h>
#include <sys/signalvar.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/stat.h>
#include <sys/syscallsubr.h>
#include <sys/sysproto.h>
#include <sys/uio.h>
#include <sys/ktrace.h>		/* Must come after sys/uio.h */
#include <sys/un.h>

#include <netinet/in.h>

#include <compat/svr4/svr4.h>
#include <compat/svr4/svr4_types.h>
#include <compat/svr4/svr4_util.h>
#include <compat/svr4/svr4_signal.h>
#include <compat/svr4/svr4_proto.h>
#include <compat/svr4/svr4_stropts.h>
#include <compat/svr4/svr4_timod.h>
#include <compat/svr4/svr4_sockmod.h>
#include <compat/svr4/svr4_ioctl.h>
#include <compat/svr4/svr4_socket.h>

/* Utils */
static int clean_pipe(struct thread *, char *);
static void getparm(struct file *, struct svr4_si_sockparms *);
static int svr4_do_putmsg(struct thread *, struct svr4_sys_putmsg_args *,
			       struct file *);
static int svr4_do_getmsg(struct thread *, struct svr4_sys_getmsg_args *,
			       struct file *);

/* Address Conversions */
static void sockaddr_to_netaddr_in(struct svr4_strmcmd *,
					const struct sockaddr_in *);
static void sockaddr_to_netaddr_un(struct svr4_strmcmd *,
					const struct sockaddr_un *);
static void netaddr_to_sockaddr_in(struct sockaddr_in *,
					const struct svr4_strmcmd *);
static void netaddr_to_sockaddr_un(struct sockaddr_un *,
					const struct svr4_strmcmd *);

/* stream ioctls */
static int i_nread(struct file *, struct thread *, register_t *, int,
			u_long, caddr_t);
static int i_fdinsert(struct file *, struct thread *, register_t *, int,
			   u_long, caddr_t);
static int i_str(struct file *, struct thread *, register_t *, int,
			u_long, caddr_t);
static int i_setsig(struct file *, struct thread *, register_t *, int,
			u_long, caddr_t);
static int i_getsig(struct file *, struct thread *, register_t *, int,
			u_long, caddr_t);
static int _i_bind_rsvd(struct file *, struct thread *, register_t *, int,
			     u_long, caddr_t);
static int _i_rele_rsvd(struct file *, struct thread *, register_t *, int,
			     u_long, caddr_t);

/* i_str sockmod calls */
static int sockmod(struct file *, int, struct svr4_strioctl *,
			      struct thread *);
static int si_listen(struct file *, int, struct svr4_strioctl *,
			      struct thread *);
static int si_ogetudata(struct file *, int, struct svr4_strioctl *,
			      struct thread *);
static int si_sockparams(struct file *, int, struct svr4_strioctl *,
			      struct thread *);
static int si_shutdown	(struct file *, int, struct svr4_strioctl *,
			      struct thread *);
static int si_getudata(struct file *, int, struct svr4_strioctl *,
			      struct thread *);

/* i_str timod calls */
static int timod(struct file *, int, struct svr4_strioctl *, struct thread *);
static int ti_getinfo(struct file *, int, struct svr4_strioctl *,
			      struct thread *);
static int ti_bind(struct file *, int, struct svr4_strioctl *, struct thread *);

#ifdef DEBUG_SVR4
static void bufprint(u_char *, size_t);
static int show_ioc(const char *, struct svr4_strioctl *);
static int show_strbuf(struct svr4_strbuf *);
static void show_msg(const char *, int, struct svr4_strbuf *, 
			  struct svr4_strbuf *, int);

static void
bufprint(buf, len)
	u_char *buf;
	size_t len;
{
	size_t i;

	uprintf("\n\t");
	for (i = 0; i < len; i++) {
		uprintf("%x ", buf[i]);
		if (i && (i % 16) == 0) 
			uprintf("\n\t");
	}
}

static int
show_ioc(str, ioc)
	const char		*str;
	struct svr4_strioctl	*ioc;
{
	u_char *ptr = NULL;
	int len;
	int error;

	len = ioc->len;
	if (len > 1024)
		len = 1024;

	if (len > 0) {
		ptr = (u_char *) malloc(len, M_TEMP, M_WAITOK);
		if ((error = copyin(ioc->buf, ptr, len)) != 0) {
			free((char *) ptr, M_TEMP);
			return error;
		}
	}

	uprintf("%s cmd = %ld, timeout = %d, len = %d, buf = %p { ",
	    str, ioc->cmd, ioc->timeout, ioc->len, ioc->buf);

	if (ptr != NULL)
		bufprint(ptr, len);

	uprintf("}\n");

	if (ptr != NULL)
		free((char *) ptr, M_TEMP);
	return 0;
}


static int
show_strbuf(str)
	struct svr4_strbuf *str;
{
	int error;
	u_char *ptr = NULL;
	int maxlen = str->maxlen;
	int len = str->len;

	if (maxlen > 8192)
		maxlen = 8192;

	if (maxlen < 0)
		maxlen = 0;

	if (len >= maxlen)
		len = maxlen;

	if (len > 0) {
	    ptr = (u_char *) malloc(len, M_TEMP, M_WAITOK);

	    if ((error = copyin(str->buf, ptr, len)) != 0) {
		    free((char *) ptr, M_TEMP);
		    return error;
	    }
	}

	uprintf(", { %d, %d, %p=[ ", str->maxlen, str->len, str->buf);

	if (ptr)
		bufprint(ptr, len);

	uprintf("]}");

	if (ptr)
		free((char *) ptr, M_TEMP);

	return 0;
}


static void
show_msg(str, fd, ctl, dat, flags)
	const char		*str;
	int			 fd;
	struct svr4_strbuf	*ctl;
	struct svr4_strbuf	*dat;
	int			 flags;
{
	struct svr4_strbuf	buf;
	int error;

	uprintf("%s(%d", str, fd);
	if (ctl != NULL) {
		if ((error = copyin(ctl, &buf, sizeof(buf))) != 0)
			return;
		show_strbuf(&buf);
	}
	else 
		uprintf(", NULL");

	if (dat != NULL) {
		if ((error = copyin(dat, &buf, sizeof(buf))) != 0)
			return;
		show_strbuf(&buf);
	}
	else 
		uprintf(", NULL");

	uprintf(", %x);\n", flags);
}

#endif /* DEBUG_SVR4 */

/*
 * We are faced with an interesting situation. On svr4 unix sockets
 * are really pipes. But we really have sockets, and we might as
 * well use them. At the point where svr4 calls TI_BIND, it has
 * already created a named pipe for the socket using mknod(2).
 * We need to create a socket with the same name when we bind,
 * so we need to remove the pipe before, otherwise we'll get address
 * already in use. So we *carefully* remove the pipe, to avoid
 * using this as a random file removal tool. We use system calls
 * to avoid code duplication.
 */
static int
clean_pipe(td, path)
	struct thread *td;
	char *path;
{
	struct stat st;
	int error;

	error = kern_lstat(td, path, UIO_SYSSPACE, &st);

	/*
	 * Make sure we are dealing with a mode 0 named pipe.
	 */
	if ((st.st_mode & S_IFMT) != S_IFIFO)
		return (0);

	if ((st.st_mode & ALLPERMS) != 0)
		return (0);

	error = kern_unlink(td, path, UIO_SYSSPACE);
	if (error)
		DPRINTF(("clean_pipe: unlink failed %d\n", error));
	return (error);
}


static void
sockaddr_to_netaddr_in(sc, sain)
	struct svr4_strmcmd *sc;
	const struct sockaddr_in *sain;
{
	struct svr4_netaddr_in *na;
	na = SVR4_ADDROF(sc);

	na->family = sain->sin_family;
	na->port = sain->sin_port;
	na->addr = sain->sin_addr.s_addr;
	DPRINTF(("sockaddr_in -> netaddr %d %d %lx\n", na->family, na->port,
		 na->addr));
}


static void
sockaddr_to_netaddr_un(sc, saun)
	struct svr4_strmcmd *sc;
	const struct sockaddr_un *saun;
{
	struct svr4_netaddr_un *na;
	char *dst, *edst = ((char *) sc) + sc->offs + sizeof(na->family) + 1  -
	    sizeof(*sc);
	const char *src;

	na = SVR4_ADDROF(sc);
	na->family = saun->sun_family;
	for (src = saun->sun_path, dst = na->path; (*dst++ = *src++) != '\0'; )
		if (dst == edst)
			break;
	DPRINTF(("sockaddr_un -> netaddr %d %s\n", na->family, na->path));
}


static void
netaddr_to_sockaddr_in(sain, sc)
	struct sockaddr_in *sain;
	const struct svr4_strmcmd *sc;
{
	const struct svr4_netaddr_in *na;


	na = SVR4_C_ADDROF(sc);
	memset(sain, 0, sizeof(*sain));
	sain->sin_len = sizeof(*sain);
	sain->sin_family = na->family;
	sain->sin_port = na->port;
	sain->sin_addr.s_addr = na->addr;
	DPRINTF(("netaddr -> sockaddr_in %d %d %x\n", sain->sin_family,
		 sain->sin_port, sain->sin_addr.s_addr));
}


static void
netaddr_to_sockaddr_un(saun, sc)
	struct sockaddr_un *saun;
	const struct svr4_strmcmd *sc;
{
	const struct svr4_netaddr_un *na;
	char *dst, *edst = &saun->sun_path[sizeof(saun->sun_path) - 1];
	const char *src;

	na = SVR4_C_ADDROF(sc);
	memset(saun, 0, sizeof(*saun));
	saun->sun_family = na->family;
	for (src = na->path, dst = saun->sun_path; (*dst++ = *src++) != '\0'; )
		if (dst == edst)
			break;
	saun->sun_len = dst - saun->sun_path;
	DPRINTF(("netaddr -> sockaddr_un %d %s\n", saun->sun_family,
		 saun->sun_path));
}


static void
getparm(fp, pa)
	struct file *fp;
	struct svr4_si_sockparms *pa;
{
	struct svr4_strm *st;
	struct socket *so;

	st = svr4_stream_get(fp);
	if (st == NULL)
		return;

	so = fp->f_data;

	pa->family = st->s_family;

	switch (so->so_type) {
	case SOCK_DGRAM:
		pa->type = SVR4_T_CLTS;
		pa->protocol = IPPROTO_UDP;
		DPRINTF(("getparm(dgram)\n"));
		return;

	case SOCK_STREAM:
	        pa->type = SVR4_T_COTS;  /* What about T_COTS_ORD? XXX */
		pa->protocol = IPPROTO_IP;
		DPRINTF(("getparm(stream)\n"));
		return;

	case SOCK_RAW:
		pa->type = SVR4_T_CLTS;
		pa->protocol = IPPROTO_RAW;
		DPRINTF(("getparm(raw)\n"));
		return;

	default:
		pa->type = 0;
		pa->protocol = 0;
		DPRINTF(("getparm(type %d?)\n", so->so_type));
		return;
	}
}


static int
si_ogetudata(fp, fd, ioc, td)
	struct file		*fp;
	int 			 fd;
	struct svr4_strioctl	*ioc;
	struct thread		*td;
{
	int error;
	struct svr4_si_oudata ud;
	struct svr4_si_sockparms pa;

	if (ioc->len != sizeof(ud) && ioc->len != sizeof(ud) - sizeof(int)) {
		DPRINTF(("SI_OGETUDATA: Wrong size %d != %d\n",
			 sizeof(ud), ioc->len));
		return EINVAL;
	}

	if ((error = copyin(ioc->buf, &ud, sizeof(ud))) != 0)
		return error;

	getparm(fp, &pa);

	switch (pa.family) {
	case AF_INET:
	    ud.tidusize = 16384;
	    ud.addrsize = sizeof(struct svr4_sockaddr_in);
	    if (pa.type == SVR4_SOCK_STREAM) 
		    ud.etsdusize = 1;
	    else
		    ud.etsdusize = 0;
	    break;

	case AF_LOCAL:
	    ud.tidusize = 65536;
	    ud.addrsize = 128;
	    ud.etsdusize = 128;
	    break;

	default:
	    DPRINTF(("SI_OGETUDATA: Unsupported address family %d\n",
		     pa.family));
	    return ENOSYS;
	}

	/* I have no idea what these should be! */
	ud.optsize = 128;
	ud.tsdusize = 128;

	ud.servtype = pa.type;

	/* XXX: Fixme */
	ud.so_state = 0;
	ud.so_options = 0;
	return copyout(&ud, ioc->buf, ioc->len);
}


static int
si_sockparams(fp, fd, ioc, td)
	struct file		*fp;
	int 			 fd;
	struct svr4_strioctl	*ioc;
	struct thread		*td;
{
	struct svr4_si_sockparms pa;

	getparm(fp, &pa);
	return copyout(&pa, ioc->buf, sizeof(pa));
}


static int
si_listen(fp, fd, ioc, td)
	struct file		*fp;
	int 			 fd;
	struct svr4_strioctl	*ioc;
	struct thread		*td;
{
	int error;
	struct svr4_strm *st = svr4_stream_get(fp);
	struct svr4_strmcmd lst;
	struct listen_args la;

	if (st == NULL)
		return EINVAL;

	if (ioc->len < 0 || ioc->len > sizeof(lst))
		return EINVAL;

	if ((error = copyin(ioc->buf, &lst, ioc->len)) != 0)
		return error;

	if (lst.cmd != SVR4_TI_OLD_BIND_REQUEST) {
		DPRINTF(("si_listen: bad request %ld\n", lst.cmd));
		return EINVAL;
	}

	/*
	 * We are making assumptions again...
	 */
	la.s = fd;
	DPRINTF(("SI_LISTEN: fileno %d backlog = %d\n", fd, 5));
	la.backlog = 5;

	if ((error = sys_listen(td, &la)) != 0) {
		DPRINTF(("SI_LISTEN: listen failed %d\n", error));
		return error;
	}

	st->s_cmd = SVR4_TI__ACCEPT_WAIT;
	lst.cmd = SVR4_TI_BIND_REPLY;

	switch (st->s_family) {
	case AF_INET:
		/* XXX: Fill the length here */
		break;

	case AF_LOCAL:
		lst.len = 140;
		lst.pad[28] = 0x00000000;	/* magic again */
		lst.pad[29] = 0x00000800;	/* magic again */
		lst.pad[30] = 0x80001400;	/* magic again */
		break;

	default:
		DPRINTF(("SI_LISTEN: Unsupported address family %d\n",
		    st->s_family));
		return ENOSYS;
	}


	if ((error = copyout(&lst, ioc->buf, ioc->len)) != 0)
		return error;

	return 0;
}


static int
si_getudata(fp, fd, ioc, td)
	struct file		*fp;
	int 			 fd;
	struct svr4_strioctl	*ioc;
	struct thread		*td;
{
	int error;
	struct svr4_si_udata ud;

	if (sizeof(ud) != ioc->len) {
		DPRINTF(("SI_GETUDATA: Wrong size %d != %d\n",
			 sizeof(ud), ioc->len));
		return EINVAL;
	}

	if ((error = copyin(ioc->buf, &ud, sizeof(ud))) != 0)
		return error;

	getparm(fp, &ud.sockparms);

	switch (ud.sockparms.family) {
	case AF_INET:
	    DPRINTF(("getudata_inet\n"));
	    ud.tidusize = 16384;
	    ud.tsdusize = 16384;
	    ud.addrsize = sizeof(struct svr4_sockaddr_in);
	    if (ud.sockparms.type == SVR4_SOCK_STREAM) 
		    ud.etsdusize = 1;
	    else
		    ud.etsdusize = 0;
	    ud.optsize = 0;
	    break;

	case AF_LOCAL:
	    DPRINTF(("getudata_local\n"));
	    ud.tidusize = 65536;
	    ud.tsdusize = 128;
	    ud.addrsize = 128;
	    ud.etsdusize = 128;
	    ud.optsize = 128;
	    break;

	default:
	    DPRINTF(("SI_GETUDATA: Unsupported address family %d\n",
		     ud.sockparms.family));
	    return ENOSYS;
	}


	ud.servtype = ud.sockparms.type;
	DPRINTF(("ud.servtype = %d\n", ud.servtype));
	/* XXX: Fixme */
	ud.so_state = 0;
	ud.so_options = 0;
	return copyout(&ud, ioc->buf, sizeof(ud));
}


static int
si_shutdown(fp, fd, ioc, td)
	struct file		*fp;
	int 			 fd;
	struct svr4_strioctl	*ioc;
	struct thread		*td;
{
	int error;
	struct shutdown_args ap;

	if (ioc->len != sizeof(ap.how)) {
		DPRINTF(("SI_SHUTDOWN: Wrong size %d != %d\n",
			 sizeof(ap.how), ioc->len));
		return EINVAL;
	}

	if ((error = copyin(ioc->buf, &ap.how, ioc->len)) != 0)
		return error;

	ap.s = fd;

	return sys_shutdown(td, &ap);
}


static int
sockmod(fp, fd, ioc, td)
	struct file		*fp;
	int			 fd;
	struct svr4_strioctl	*ioc;
	struct thread		*td;
{
	switch (ioc->cmd) {
	case SVR4_SI_OGETUDATA:
		DPRINTF(("SI_OGETUDATA\n"));
		return si_ogetudata(fp, fd, ioc, td);

	case SVR4_SI_SHUTDOWN:
		DPRINTF(("SI_SHUTDOWN\n"));
		return si_shutdown(fp, fd, ioc, td);

	case SVR4_SI_LISTEN:
		DPRINTF(("SI_LISTEN\n"));
		return si_listen(fp, fd, ioc, td);

	case SVR4_SI_SETMYNAME:
		DPRINTF(("SI_SETMYNAME\n"));
		return 0;

	case SVR4_SI_SETPEERNAME:
		DPRINTF(("SI_SETPEERNAME\n"));
		return 0;

	case SVR4_SI_GETINTRANSIT:
		DPRINTF(("SI_GETINTRANSIT\n"));
		return 0;

	case SVR4_SI_TCL_LINK:
		DPRINTF(("SI_TCL_LINK\n"));
		return 0;

	case SVR4_SI_TCL_UNLINK:
		DPRINTF(("SI_TCL_UNLINK\n"));
		return 0;

	case SVR4_SI_SOCKPARAMS:
		DPRINTF(("SI_SOCKPARAMS\n"));
		return si_sockparams(fp, fd, ioc, td);

	case SVR4_SI_GETUDATA:
		DPRINTF(("SI_GETUDATA\n"));
		return si_getudata(fp, fd, ioc, td);

	default:
		DPRINTF(("Unknown sockmod ioctl %lx\n", ioc->cmd));
		return 0;

	}
}


static int
ti_getinfo(fp, fd, ioc, td)
	struct file		*fp;
	int 			 fd;
	struct svr4_strioctl	*ioc;
	struct thread		*td;
{
	int error;
	struct svr4_infocmd info;

	memset(&info, 0, sizeof(info));

	if (ioc->len < 0 || ioc->len > sizeof(info))
		return EINVAL;

	if ((error = copyin(ioc->buf, &info, ioc->len)) != 0)
		return error;

	if (info.cmd != SVR4_TI_INFO_REQUEST)
		return EINVAL;

	info.cmd = SVR4_TI_INFO_REPLY;
	info.tsdu = 0;
	info.etsdu = 1;
	info.cdata = -2;
	info.ddata = -2;
	info.addr = 16;
	info.opt = -1;
	info.tidu = 16384;
	info.serv = 2;
	info.current = 0;
	info.provider = 2;

	ioc->len = sizeof(info);
	if ((error = copyout(&info, ioc->buf, ioc->len)) != 0)
		return error;

	return 0;
}


static int
ti_bind(fp, fd, ioc, td)
	struct file		*fp;
	int 			 fd;
	struct svr4_strioctl	*ioc;
	struct thread		*td;
{
	int error;
	struct svr4_strm *st = svr4_stream_get(fp);
	struct sockaddr_in sain;
	struct sockaddr_un saun;
	struct sockaddr *skp;
	int sasize;
	struct svr4_strmcmd bnd;

	if (st == NULL) {
		DPRINTF(("ti_bind: bad file descriptor\n"));
		return EINVAL;
	}

	if (ioc->len < 0 || ioc->len > sizeof(bnd))
		return EINVAL;

	if ((error = copyin(ioc->buf, &bnd, ioc->len)) != 0)
		return error;

	if (bnd.cmd != SVR4_TI_OLD_BIND_REQUEST) {
		DPRINTF(("ti_bind: bad request %ld\n", bnd.cmd));
		return EINVAL;
	}

	switch (st->s_family) {
	case AF_INET:
		skp = (struct sockaddr *)&sain;
		sasize = sizeof(sain);

		if (bnd.offs == 0)
			goto error;

		netaddr_to_sockaddr_in(&sain, &bnd);

		DPRINTF(("TI_BIND: fam %d, port %d, addr %x\n",
			 sain.sin_family, sain.sin_port,
			 sain.sin_addr.s_addr));
		break;

	case AF_LOCAL:
		skp = (struct sockaddr *)&saun;
		sasize = sizeof(saun);
		if (bnd.offs == 0)
			goto error;

		netaddr_to_sockaddr_un(&saun, &bnd);

		if (saun.sun_path[0] == '\0')
			goto error;

		DPRINTF(("TI_BIND: fam %d, path %s\n",
			 saun.sun_family, saun.sun_path));

		if ((error = clean_pipe(td, saun.sun_path)) != 0)
			return error;

		bnd.pad[28] = 0x00001000;	/* magic again */
		break;

	default:
		DPRINTF(("TI_BIND: Unsupported address family %d\n",
			 st->s_family));
		return ENOSYS;
	}

	DPRINTF(("TI_BIND: fileno %d\n", fd));

	if ((error = kern_bind(td, fd, skp)) != 0) {
		DPRINTF(("TI_BIND: bind failed %d\n", error));
		return error;
	}
	goto reply;

error:
	memset(&bnd, 0, sizeof(bnd));
	bnd.len = sasize + 4;
	bnd.offs = 0x10;	/* XXX */

reply:
	bnd.cmd = SVR4_TI_BIND_REPLY;

	if ((error = copyout(&bnd, ioc->buf, ioc->len)) != 0)
		return error;

	return 0;
}


static int
timod(fp, fd, ioc, td)
	struct file		*fp;
	int			 fd;
	struct svr4_strioctl	*ioc;
	struct thread		*td;
{
	switch (ioc->cmd) {
	case SVR4_TI_GETINFO:
		DPRINTF(("TI_GETINFO\n"));
		return ti_getinfo(fp, fd, ioc, td);

	case SVR4_TI_OPTMGMT:
		DPRINTF(("TI_OPTMGMT\n"));
		return 0;

	case SVR4_TI_BIND:
		DPRINTF(("TI_BIND\n"));
		return ti_bind(fp, fd, ioc, td);

	case SVR4_TI_UNBIND:
		DPRINTF(("TI_UNBIND\n"));
		return 0;

	default:
		DPRINTF(("Unknown timod ioctl %lx\n", ioc->cmd));
		return 0;
	}
}


int
svr4_stream_ti_ioctl(fp, td, retval, fd, cmd, dat)
	struct file *fp;
	struct thread *td;
	register_t *retval;
	int fd;
	u_long cmd;
	caddr_t dat;
{
	struct svr4_strbuf skb, *sub = (struct svr4_strbuf *) dat;
	struct svr4_strm *st = svr4_stream_get(fp);
	int error;
	struct sockaddr *sa;
	socklen_t sasize, oldsasize;
	struct svr4_strmcmd sc;

	DPRINTF(("svr4_stream_ti_ioctl\n"));

	if (st == NULL)
		return EINVAL;

	sc.offs = 0x10;
	
	if ((error = copyin(sub, &skb, sizeof(skb))) != 0) {
		DPRINTF(("ti_ioctl: error copying in strbuf\n"));
		return error;
	}

	switch (st->s_family) {
	case AF_INET:
		sasize = sizeof(struct sockaddr_in);
		break;

	case AF_LOCAL:
		sasize = sizeof(struct sockaddr_un);
		break;

	default:
		DPRINTF(("ti_ioctl: Unsupported address family %d\n",
			 st->s_family));
		return ENOSYS;
	}
	oldsasize = sasize;

	switch (cmd) {
	case SVR4_TI_GETMYNAME:
		DPRINTF(("TI_GETMYNAME\n"));
		{
			error = kern_getsockname(td, fd, &sa, &sasize);
			if (error) {
				DPRINTF(("ti_ioctl: getsockname error\n"));
				return error;
			}
		}
		break;

	case SVR4_TI_GETPEERNAME:
		DPRINTF(("TI_GETPEERNAME\n"));
		{
			error = kern_getpeername(td, fd, &sa, &sasize);
			if (error) {
				DPRINTF(("ti_ioctl: getpeername error\n"));
				return error;
			}
		}
		break;

	case SVR4_TI_SETMYNAME:
		DPRINTF(("TI_SETMYNAME\n"));
		return 0;

	case SVR4_TI_SETPEERNAME:
		DPRINTF(("TI_SETPEERNAME\n"));
		return 0;
	default:
		DPRINTF(("ti_ioctl: Unknown ioctl %lx\n", cmd));
		return ENOSYS;
	}

	if (sasize < 0 || sasize > oldsasize) {
		free(sa, M_SONAME);
		return EINVAL;
	}

	switch (st->s_family) {
	case AF_INET:
		sockaddr_to_netaddr_in(&sc, (struct sockaddr_in *)sa);
		skb.len = sasize;
		break;

	case AF_LOCAL:
		sockaddr_to_netaddr_un(&sc, (struct sockaddr_un *)sa);
		skb.len = sasize + 4;
		break;

	default:
		free(sa, M_SONAME);
		return ENOSYS;
	}
	free(sa, M_SONAME);

	if ((error = copyout(SVR4_ADDROF(&sc), skb.buf, sasize)) != 0) {
		DPRINTF(("ti_ioctl: error copying out socket data\n"));
		return error;
	}


	if ((error = copyout(&skb, sub, sizeof(skb))) != 0) {
		DPRINTF(("ti_ioctl: error copying out strbuf\n"));
		return error;
	}

	return error;
}




static int
i_nread(fp, td, retval, fd, cmd, dat)
	struct file *fp;
	struct thread *td;
	register_t *retval;
	int fd;
	u_long cmd;
	caddr_t dat;
{
	int error;
	int nread = 0;	

	/*
	 * We are supposed to return the message length in nread, and the
	 * number of messages in retval. We don't have the notion of number
	 * of stream messages, so we just find out if we have any bytes waiting
	 * for us, and if we do, then we assume that we have at least one
	 * message waiting for us.
	 */
	if ((error = fo_ioctl(fp, FIONREAD, (caddr_t) &nread, td->td_ucred,
	    td)) != 0)
		return error;

	if (nread != 0)
		*retval = 1;
	else
		*retval = 0;

	return copyout(&nread, dat, sizeof(nread));
}

static int
i_fdinsert(fp, td, retval, fd, cmd, dat)
	struct file *fp;
	struct thread *td;
	register_t *retval;
	int fd;
	u_long cmd;
	caddr_t dat;
{
	/*
	 * Major hack again here. We assume that we are using this to
	 * implement accept(2). If that is the case, we have already
	 * called accept, and we have stored the file descriptor in
	 * afd. We find the file descriptor that the code wants to use
	 * in fd insert, and then we dup2() our accepted file descriptor
	 * to it.
	 */
	int error;
	struct svr4_strm *st = svr4_stream_get(fp);
	struct svr4_strfdinsert fdi;
	struct dup2_args d2p;

	if (st == NULL) {
		DPRINTF(("fdinsert: bad file type\n"));
		return EINVAL;
	}

	mtx_lock(&Giant);
	if (st->s_afd == -1) {
		DPRINTF(("fdinsert: accept fd not found\n"));
		mtx_unlock(&Giant);
		return ENOENT;
	}

	if ((error = copyin(dat, &fdi, sizeof(fdi))) != 0) {
		DPRINTF(("fdinsert: copyin failed %d\n", error));
		mtx_unlock(&Giant);
		return error;
	}

	d2p.from = st->s_afd;
	d2p.to = fdi.fd;

	if ((error = sys_dup2(td, &d2p)) != 0) {
		DPRINTF(("fdinsert: dup2(%d, %d) failed %d\n", 
		    st->s_afd, fdi.fd, error));
		mtx_unlock(&Giant);
		return error;
	}

	if ((error = kern_close(td, st->s_afd)) != 0) {
		DPRINTF(("fdinsert: close(%d) failed %d\n", 
		    st->s_afd, error));
		mtx_unlock(&Giant);
		return error;
	}

	st->s_afd = -1;
	mtx_unlock(&Giant);

	*retval = 0;
	return 0;
}


static int
_i_bind_rsvd(fp, td, retval, fd, cmd, dat)
	struct file *fp;
	struct thread *td;
	register_t *retval;
	int fd;
	u_long cmd;
	caddr_t dat;
{
	struct mkfifo_args ap;

	/*
	 * This is a supposed to be a kernel and library only ioctl.
	 * It gets called before ti_bind, when we have a unix 
	 * socket, to physically create the socket transport and
	 * ``reserve'' it. I don't know how this get reserved inside
	 * the kernel, but we are going to create it nevertheless.
	 */
	ap.path = dat;
	ap.mode = S_IFIFO;

	return sys_mkfifo(td, &ap);
}

static int
_i_rele_rsvd(fp, td, retval, fd, cmd, dat)
	struct file *fp;
	struct thread *td;
	register_t *retval;
	int fd;
	u_long cmd;
	caddr_t dat;
{
	struct unlink_args ap;

	/*
	 * This is a supposed to be a kernel and library only ioctl.
	 * I guess it is supposed to release the socket.
	 */
	ap.path = dat;

	return sys_unlink(td, &ap);
}

static int
i_str(fp, td, retval, fd, cmd, dat)
	struct file *fp;
	struct thread *td;
	register_t *retval;
	int fd;
	u_long cmd;
	caddr_t dat;
{
	int			 error;
	struct svr4_strioctl	 ioc;

	if ((error = copyin(dat, &ioc, sizeof(ioc))) != 0)
		return error;

#ifdef DEBUG_SVR4
	if ((error = show_ioc(">", &ioc)) != 0)
		return error;
#endif /* DEBUG_SVR4 */

	switch (ioc.cmd & 0xff00) {
	case SVR4_SIMOD:
		if ((error = sockmod(fp, fd, &ioc, td)) != 0)
			return error;
		break;

	case SVR4_TIMOD:
		if ((error = timod(fp, fd, &ioc, td)) != 0)
			return error;
		break;

	default:
		DPRINTF(("Unimplemented module %c %ld\n",
			 (char) (cmd >> 8), cmd & 0xff));
		return 0;
	}

#ifdef DEBUG_SVR4
	if ((error = show_ioc("<", &ioc)) != 0)
		return error;
#endif /* DEBUG_SVR4 */
	return copyout(&ioc, dat, sizeof(ioc));
}

static int
i_setsig(fp, td, retval, fd, cmd, dat)
	struct file *fp;
	struct thread *td;
	register_t *retval;
	int fd;
	u_long cmd;
	caddr_t dat;
{
	/* 
	 * This is the best we can do for now; we cannot generate
	 * signals only for specific events so the signal mask gets
	 * ignored; we save it just to pass it to a possible I_GETSIG...
	 *
	 * We alse have to fix the O_ASYNC fcntl bit, so the
	 * process will get SIGPOLLs.
	 */
	int error;
	register_t oflags, flags;
	struct svr4_strm *st = svr4_stream_get(fp);

	if (st == NULL) {
		DPRINTF(("i_setsig: bad file descriptor\n"));
		return EINVAL;
	}
	/* get old status flags */
	error = kern_fcntl(td, fd, F_GETFL, 0);
	if (error)
		return (error);

	oflags = td->td_retval[0];

	/* update the flags */
	mtx_lock(&Giant);
	if (dat != NULL) {
		int mask;

		flags = oflags | O_ASYNC;
		if ((error = copyin(dat, &mask, sizeof(mask))) != 0) {
			  DPRINTF(("i_setsig: bad eventmask pointer\n"));
			  return error;
		}
		if (mask & SVR4_S_ALLMASK) {
			  DPRINTF(("i_setsig: bad eventmask data %x\n", mask));
			  return EINVAL;
		}
		st->s_eventmask = mask;
	}
	else {
		flags = oflags & ~O_ASYNC;
		st->s_eventmask = 0;
	}
	mtx_unlock(&Giant);

	/* set the new flags, if changed */
	if (flags != oflags) {
		error = kern_fcntl(td, fd, F_SETFL, flags);
		if (error)
			return (error);
		flags = td->td_retval[0];
	}

	/* set up SIGIO receiver if needed */
	if (dat != NULL)
		return (kern_fcntl(td, fd, F_SETOWN, td->td_proc->p_pid));
	return 0;
}

static int
i_getsig(fp, td, retval, fd, cmd, dat)
	struct file *fp;
	struct thread *td;
	register_t *retval;
	int fd;
	u_long cmd;
	caddr_t dat;
{
	int error, eventmask;

	if (dat != NULL) {
		struct svr4_strm *st = svr4_stream_get(fp);

		if (st == NULL) {
			DPRINTF(("i_getsig: bad file descriptor\n"));
			return EINVAL;
		}
		mtx_lock(&Giant);
		eventmask = st->s_eventmask;
		mtx_unlock(&Giant);		
		if ((error = copyout(&eventmask, dat,
				     sizeof(eventmask))) != 0) {
			DPRINTF(("i_getsig: bad eventmask pointer\n"));
			return error;
		}
	}
	return 0;
}

int
svr4_stream_ioctl(fp, td, retval, fd, cmd, dat)
	struct file *fp;
	struct thread *td;
	register_t *retval;
	int fd;
	u_long cmd;
	caddr_t dat;
{
	*retval = 0;

	/*
	 * All the following stuff assumes "sockmod" is pushed...
	 */
	switch (cmd) {
	case SVR4_I_NREAD:
		DPRINTF(("I_NREAD\n"));
		return i_nread(fp, td, retval, fd, cmd, dat);

	case SVR4_I_PUSH:
		DPRINTF(("I_PUSH %p\n", dat));
#if defined(DEBUG_SVR4)
		show_strbuf((struct svr4_strbuf *)dat);
#endif
		return 0;

	case SVR4_I_POP:
		DPRINTF(("I_POP\n"));
		return 0;

	case SVR4_I_LOOK:
		DPRINTF(("I_LOOK\n"));
		return 0;

	case SVR4_I_FLUSH:
		DPRINTF(("I_FLUSH\n"));
		return 0;

	case SVR4_I_SRDOPT:
		DPRINTF(("I_SRDOPT\n"));
		return 0;

	case SVR4_I_GRDOPT:
		DPRINTF(("I_GRDOPT\n"));
		return 0;

	case SVR4_I_STR:
		DPRINTF(("I_STR\n"));
		return i_str(fp, td, retval, fd, cmd, dat);

	case SVR4_I_SETSIG:
		DPRINTF(("I_SETSIG\n"));
		return i_setsig(fp, td, retval, fd, cmd, dat);

	case SVR4_I_GETSIG:
	        DPRINTF(("I_GETSIG\n"));
		return i_getsig(fp, td, retval, fd, cmd, dat);

	case SVR4_I_FIND:
		DPRINTF(("I_FIND\n"));
		/*
		 * Here we are not pushing modules really, we just
		 * pretend all are present
		 */
		*retval = 0;
		return 0;

	case SVR4_I_LINK:
		DPRINTF(("I_LINK\n"));
		return 0;

	case SVR4_I_UNLINK:
		DPRINTF(("I_UNLINK\n"));
		return 0;

	case SVR4_I_ERECVFD:
		DPRINTF(("I_ERECVFD\n"));
		return 0;

	case SVR4_I_PEEK:
		DPRINTF(("I_PEEK\n"));
		return 0;

	case SVR4_I_FDINSERT:
		DPRINTF(("I_FDINSERT\n"));
		return i_fdinsert(fp, td, retval, fd, cmd, dat);

	case SVR4_I_SENDFD:
		DPRINTF(("I_SENDFD\n"));
		return 0;

	case SVR4_I_RECVFD:
		DPRINTF(("I_RECVFD\n"));
		return 0;

	case SVR4_I_SWROPT:
		DPRINTF(("I_SWROPT\n"));
		return 0;

	case SVR4_I_GWROPT:
		DPRINTF(("I_GWROPT\n"));
		return 0;

	case SVR4_I_LIST:
		DPRINTF(("I_LIST\n"));
		return 0;

	case SVR4_I_PLINK:
		DPRINTF(("I_PLINK\n"));
		return 0;

	case SVR4_I_PUNLINK:
		DPRINTF(("I_PUNLINK\n"));
		return 0;

	case SVR4_I_SETEV:
		DPRINTF(("I_SETEV\n"));
		return 0;

	case SVR4_I_GETEV:
		DPRINTF(("I_GETEV\n"));
		return 0;

	case SVR4_I_STREV:
		DPRINTF(("I_STREV\n"));
		return 0;

	case SVR4_I_UNSTREV:
		DPRINTF(("I_UNSTREV\n"));
		return 0;

	case SVR4_I_FLUSHBAND:
		DPRINTF(("I_FLUSHBAND\n"));
		return 0;

	case SVR4_I_CKBAND:
		DPRINTF(("I_CKBAND\n"));
		return 0;

	case SVR4_I_GETBAND:
		DPRINTF(("I_GETBANK\n"));
		return 0;

	case SVR4_I_ATMARK:
		DPRINTF(("I_ATMARK\n"));
		return 0;

	case SVR4_I_SETCLTIME:
		DPRINTF(("I_SETCLTIME\n"));
		return 0;

	case SVR4_I_GETCLTIME:
		DPRINTF(("I_GETCLTIME\n"));
		return 0;

	case SVR4_I_CANPUT:
		DPRINTF(("I_CANPUT\n"));
		return 0;

	case SVR4__I_BIND_RSVD:
		DPRINTF(("_I_BIND_RSVD\n"));
		return _i_bind_rsvd(fp, td, retval, fd, cmd, dat);

	case SVR4__I_RELE_RSVD:
		DPRINTF(("_I_RELE_RSVD\n"));
		return _i_rele_rsvd(fp, td, retval, fd, cmd, dat);

	default:
		DPRINTF(("unimpl cmd = %lx\n", cmd));
		break;
	}

	return 0;
}



int
svr4_sys_putmsg(td, uap)
	struct thread *td;
	struct svr4_sys_putmsg_args *uap;
{
	struct file     *fp;
	int error;

	if ((error = fget(td, uap->fd, CAP_SEND, &fp)) != 0) {
#ifdef DEBUG_SVR4
	        uprintf("putmsg: bad fp\n");
#endif
		return EBADF;
	}
	error = svr4_do_putmsg(td, uap, fp);
	fdrop(fp, td);
	return (error);
}

static int
svr4_do_putmsg(td, uap, fp)
	struct thread *td;
	struct svr4_sys_putmsg_args *uap;
	struct file	*fp;
{
	struct svr4_strbuf dat, ctl;
	struct svr4_strmcmd sc;
	struct sockaddr_in sain;
	struct sockaddr_un saun;
	struct sockaddr *sa;
	int sasize, *retval;
	struct svr4_strm *st;
	int error;

	retval = td->td_retval;

#ifdef DEBUG_SVR4
	show_msg(">putmsg", uap->fd, uap->ctl,
		 uap->dat, uap->flags);
#endif /* DEBUG_SVR4 */

	if (uap->ctl != NULL) {
	  if ((error = copyin(uap->ctl, &ctl, sizeof(ctl))) != 0) {
#ifdef DEBUG_SVR4
	    uprintf("putmsg: copyin(): %d\n", error);
#endif
	    return error;
	  }
	}
	else
		ctl.len = -1;

	if (uap->dat != NULL) {
	  if ((error = copyin(uap->dat, &dat, sizeof(dat))) != 0) {
#ifdef DEBUG_SVR4
	    uprintf("putmsg: copyin(): %d (2)\n", error);
#endif
	    return error;
	  }
	}
	else
		dat.len = -1;

	/*
	 * Only for sockets for now.
	 */
	if ((st = svr4_stream_get(fp)) == NULL) {
		DPRINTF(("putmsg: bad file type\n"));
		return EINVAL;
	}

	if (ctl.len < 0 || ctl.len > sizeof(sc)) {
		DPRINTF(("putmsg: Bad control size %d != %d\n", ctl.len,
			 sizeof(struct svr4_strmcmd)));
		return EINVAL;
	}

	if ((error = copyin(ctl.buf, &sc, ctl.len)) != 0)
		return error;

	switch (st->s_family) {
	case AF_INET:
	        if (sc.len != sizeof(sain)) {
		        if (sc.cmd == SVR4_TI_DATA_REQUEST) {
			        struct write_args wa;

				/* Solaris seems to use sc.cmd = 3 to
				 * send "expedited" data.  telnet uses
				 * this for options processing, sending EOF,
				 * etc.  I'm sure other things use it too.
				 * I don't have any documentation
				 * on it, so I'm making a guess that this
				 * is how it works. newton@atdot.dotat.org XXX
				 */
				DPRINTF(("sending expedited data ??\n"));
				wa.fd = uap->fd;
				wa.buf = dat.buf;
				wa.nbyte = dat.len;
				return sys_write(td, &wa);
			}
	                DPRINTF(("putmsg: Invalid inet length %ld\n", sc.len));
	                return EINVAL;
	        }
	        netaddr_to_sockaddr_in(&sain, &sc);
		sa = (struct sockaddr *)&sain;
	        sasize = sizeof(sain);
		if (sain.sin_family != st->s_family)
			error = EINVAL;
		break;

	case AF_LOCAL:
		if (ctl.len == 8) {
			/* We are doing an accept; succeed */
			DPRINTF(("putmsg: Do nothing\n"));
			*retval = 0;
			return 0;
		}
		else {
			/* Maybe we've been given a device/inode pair */
			dev_t *dev = SVR4_ADDROF(&sc);
			ino_t *ino = (ino_t *) &dev[1];
			if (svr4_find_socket(td, fp, *dev, *ino, &saun) != 0) {
				/* I guess we have it by name */
				netaddr_to_sockaddr_un(&saun, &sc);
			}
			sa = (struct sockaddr *)&saun;
			sasize = sizeof(saun);
		}
		break;

	default:
		DPRINTF(("putmsg: Unsupported address family %d\n",
			 st->s_family));
		return ENOSYS;
	}

	mtx_lock(&Giant);
	st->s_cmd = sc.cmd;
	mtx_unlock(&Giant);
	switch (sc.cmd) {
	case SVR4_TI_CONNECT_REQUEST:	/* connect 	*/
		{

			return (kern_connect(td, uap->fd, sa));
		}

	case SVR4_TI_SENDTO_REQUEST:	/* sendto 	*/
		{
			struct msghdr msg;
			struct iovec aiov;

			msg.msg_name = sa;
			msg.msg_namelen = sasize;
			msg.msg_iov = &aiov;
			msg.msg_iovlen = 1;
			msg.msg_control = 0;
			msg.msg_flags = 0;
			aiov.iov_base = dat.buf;
			aiov.iov_len = dat.len;
			error = kern_sendit(td, uap->fd, &msg, uap->flags,
			    NULL, UIO_USERSPACE);
			DPRINTF(("sendto_request error: %d\n", error));
			*retval = 0;
			return error;
		}

	default:
		DPRINTF(("putmsg: Unimplemented command %lx\n", sc.cmd));
		return ENOSYS;
	}
}

int
svr4_sys_getmsg(td, uap)
	struct thread *td;
	struct svr4_sys_getmsg_args *uap;
{
	struct file     *fp;
	int error;

	if ((error = fget(td, uap->fd, CAP_RECV, &fp)) != 0) {
#ifdef DEBUG_SVR4
	        uprintf("getmsg: bad fp\n");
#endif
		return EBADF;
	}
	error = svr4_do_getmsg(td, uap, fp);
	fdrop(fp, td);
	return (error);
}

int
svr4_do_getmsg(td, uap, fp)
	struct thread *td;
	struct svr4_sys_getmsg_args *uap;
	struct file *fp;
{
	struct svr4_strbuf dat, ctl;
	struct svr4_strmcmd sc;
	int error, *retval;
	struct msghdr msg;
	struct iovec aiov;
	struct sockaddr_in sain;
	struct sockaddr_un saun;
	struct sockaddr *sa;
	socklen_t sasize;
	struct svr4_strm *st;
	struct file *afp;
	int fl;

	retval = td->td_retval;
	error = 0;
	afp = NULL;

	memset(&sc, 0, sizeof(sc));

#ifdef DEBUG_SVR4
	show_msg(">getmsg", uap->fd, uap->ctl,
		 uap->dat, 0);
#endif /* DEBUG_SVR4 */

	if (uap->ctl != NULL) {
		if ((error = copyin(uap->ctl, &ctl, sizeof(ctl))) != 0)
			return error;
		if (ctl.len < 0)
			return EINVAL;
	}
	else {
		ctl.len = -1;
		ctl.maxlen = 0;
	}

	if (uap->dat != NULL) {
	    	if ((error = copyin(uap->dat, &dat, sizeof(dat))) != 0)
			return error;
	}
	else {
		dat.len = -1;
		dat.maxlen = 0;
	}

	/*
	 * Only for sockets for now.
	 */
	if ((st = svr4_stream_get(fp)) == NULL) {
		DPRINTF(("getmsg: bad file type\n"));
		return EINVAL;
	}

	if (ctl.maxlen == -1 || dat.maxlen == -1) {
		DPRINTF(("getmsg: Cannot handle -1 maxlen (yet)\n"));
		return ENOSYS;
	}

	switch (st->s_family) {
	case AF_INET:
		sasize = sizeof(sain);
		break;

	case AF_LOCAL:
		sasize = sizeof(saun);
		break;

	default:
		DPRINTF(("getmsg: Unsupported address family %d\n",
			 st->s_family));
		return ENOSYS;
	}

	mtx_lock(&Giant);
	switch (st->s_cmd) {
	case SVR4_TI_CONNECT_REQUEST:
		DPRINTF(("getmsg: TI_CONNECT_REQUEST\n"));
		/*
		 * We do the connect in one step, so the putmsg should
		 * have gotten the error.
		 */
		sc.cmd = SVR4_TI_OK_REPLY;
		sc.len = 0;

		ctl.len = 8;
		dat.len = -1;
		fl = 1;
		st->s_cmd = sc.cmd;
		break;

	case SVR4_TI_OK_REPLY:
		DPRINTF(("getmsg: TI_OK_REPLY\n"));
		/*
		 * We are immediately after a connect reply, so we send
		 * a connect verification.
		 */

		error = kern_getpeername(td, uap->fd, &sa, &sasize);
		if (error) {
			mtx_unlock(&Giant);
			DPRINTF(("getmsg: getpeername failed %d\n", error));
			return error;
		}

		sc.cmd = SVR4_TI_CONNECT_REPLY;
		sc.pad[0] = 0x4;
		sc.offs = 0x18;
		sc.pad[1] = 0x14;
		sc.pad[2] = 0x04000402;

		switch (st->s_family) {
		case AF_INET:
			sc.len = sasize;
			sockaddr_to_netaddr_in(&sc, (struct sockaddr_in *)sa);
			break;

		case AF_LOCAL:
			sc.len = sasize + 4;
			sockaddr_to_netaddr_un(&sc, (struct sockaddr_un *)sa);
			break;

		default:
			mtx_unlock(&Giant);
			free(sa, M_SONAME);
			return ENOSYS;
		}
		free(sa, M_SONAME);

		ctl.len = 40;
		dat.len = -1;
		fl = 0;
		st->s_cmd = sc.cmd;
		break;

	case SVR4_TI__ACCEPT_OK:
		DPRINTF(("getmsg: TI__ACCEPT_OK\n"));
		/*
		 * We do the connect in one step, so the putmsg should
		 * have gotten the error.
		 */
		sc.cmd = SVR4_TI_OK_REPLY;
		sc.len = 1;

		ctl.len = 8;
		dat.len = -1;
		fl = 1;
		st->s_cmd = SVR4_TI__ACCEPT_WAIT;
		break;

	case SVR4_TI__ACCEPT_WAIT:
		DPRINTF(("getmsg: TI__ACCEPT_WAIT\n"));
		/*
		 * We are after a listen, so we try to accept...
		 */

		error = kern_accept(td, uap->fd, &sa, &sasize, &afp);
		if (error) {
			mtx_unlock(&Giant);
			DPRINTF(("getmsg: accept failed %d\n", error));
			return error;
		}

		st->s_afd = *retval;

		DPRINTF(("getmsg: Accept fd = %d\n", st->s_afd));

		sc.cmd = SVR4_TI_ACCEPT_REPLY;
		sc.offs = 0x18;
		sc.pad[0] = 0x0;

		switch (st->s_family) {
		case AF_INET:
			sc.pad[1] = 0x28;
			sockaddr_to_netaddr_in(&sc, (struct sockaddr_in *)&sa);
			ctl.len = 40;
			sc.len = sasize;
			break;

		case AF_LOCAL:
			sc.pad[1] = 0x00010000;
			sc.pad[2] = 0xf6bcdaa0;	/* I don't know what that is */
			sc.pad[3] = 0x00010000;
			ctl.len = 134;
			sc.len = sasize + 4;
			break;

		default:
			fdclose(td->td_proc->p_fd, afp, st->s_afd, td);
			fdrop(afp, td);
			st->s_afd = -1;
			mtx_unlock(&Giant);
			free(sa, M_SONAME);
			return ENOSYS;
		}
		free(sa, M_SONAME);

		dat.len = -1;
		fl = 0;
		st->s_cmd = SVR4_TI__ACCEPT_OK;
		break;

	case SVR4_TI_SENDTO_REQUEST:
		DPRINTF(("getmsg: TI_SENDTO_REQUEST\n"));
		if (ctl.maxlen > 36 && ctl.len < 36)
		    ctl.len = 36;

		if (ctl.len > sizeof(sc))
			ctl.len = sizeof(sc);

		if ((error = copyin(ctl.buf, &sc, ctl.len)) != 0) {
			mtx_unlock(&Giant);
			return error;
		}

		switch (st->s_family) {
		case AF_INET:
			sa = (struct sockaddr *)&sain;
			sockaddr_to_netaddr_in(&sc, &sain);
			break;

		case AF_LOCAL:
			sa = (struct sockaddr *)&saun;
			sockaddr_to_netaddr_un(&sc, &saun);
			break;

		default:
			mtx_unlock(&Giant);
			return ENOSYS;
		}

		msg.msg_name = sa;
		msg.msg_namelen = sasize;
		msg.msg_iov = &aiov;
		msg.msg_iovlen = 1;
		msg.msg_control = 0;
		aiov.iov_base = dat.buf;
		aiov.iov_len = dat.maxlen;
		msg.msg_flags = 0;

		error = kern_recvit(td, uap->fd, &msg, UIO_SYSSPACE, NULL);

		if (error) {
			mtx_unlock(&Giant);
			DPRINTF(("getmsg: recvit failed %d\n", error));
			return error;
		}

		sc.cmd = SVR4_TI_RECVFROM_IND;

		switch (st->s_family) {
		case AF_INET:
			sc.len = sasize;
			sockaddr_to_netaddr_in(&sc, &sain);
			break;

		case AF_LOCAL:
			sc.len = sasize + 4;
			sockaddr_to_netaddr_un(&sc, &saun);
			break;

		default:
			mtx_unlock(&Giant);
			return ENOSYS;
		}

		dat.len = *retval;
		fl = 0;
		st->s_cmd = sc.cmd;
		break;

	default:
		st->s_cmd = sc.cmd;
		if (st->s_cmd == SVR4_TI_CONNECT_REQUEST) {
		        struct read_args ra;

			/* More weirdness:  Again, I can't find documentation
			 * to back this up, but when a process does a generic
			 * "getmsg()" call it seems that the command field is
			 * zero and the length of the data area is zero.  I
			 * think processes expect getmsg() to fill in dat.len
			 * after reading at most dat.maxlen octets from the
			 * stream.  Since we're using sockets I can let 
			 * read() look after it and frob return values
			 * appropriately (or inappropriately :-)
			 *   -- newton@atdot.dotat.org        XXX
			 */
			ra.fd = uap->fd;
			ra.buf = dat.buf;
			ra.nbyte = dat.maxlen;
			if ((error = sys_read(td, &ra)) != 0) {
				mtx_unlock(&Giant);
			        return error;
			}
			dat.len = *retval;
			*retval = 0;
			st->s_cmd = SVR4_TI_SENDTO_REQUEST;
			break;
		}
		mtx_unlock(&Giant);
		DPRINTF(("getmsg: Unknown state %x\n", st->s_cmd));
		return EINVAL;
	}

	if (uap->ctl) {
		if (ctl.len > sizeof(sc))
			ctl.len = sizeof(sc);
		if (ctl.len != -1)
			error = copyout(&sc, ctl.buf, ctl.len);

		if (error == 0)
			error = copyout(&ctl, uap->ctl, sizeof(ctl));
	}

	if (uap->dat) {
		if (error == 0)
			error = copyout(&dat, uap->dat, sizeof(dat));
	}

	if (uap->flags) { /* XXX: Need translation */
		if (error == 0)
			error = copyout(&fl, uap->flags, sizeof(fl));
	}

	if (error) {
		if (afp) {
			fdclose(td->td_proc->p_fd, afp, st->s_afd, td);
			fdrop(afp, td);
			st->s_afd = -1;
		}
		mtx_unlock(&Giant);
		return (error);
	}
	mtx_unlock(&Giant);
	if (afp)
		fdrop(afp, td);

	*retval = 0;

#ifdef DEBUG_SVR4
	show_msg("<getmsg", uap->fd, uap->ctl,
		 uap->dat, fl);
#endif /* DEBUG_SVR4 */
	return error;
}

int svr4_sys_send(td, uap)
	struct thread *td;
	struct svr4_sys_send_args *uap;
{
	struct sendto_args sta;

	sta.s = uap->s;
	sta.buf = uap->buf;
	sta.len = uap->len;
	sta.flags = uap->flags;
	sta.to = NULL;
	sta.tolen = 0;

	return (sys_sendto(td, &sta));
}

int svr4_sys_recv(td, uap)
	struct thread *td;
	struct svr4_sys_recv_args *uap;
{
	struct recvfrom_args rfa;

	rfa.s = uap->s;
	rfa.buf = uap->buf;
	rfa.len = uap->len;
	rfa.flags = uap->flags;
	rfa.from = NULL;
	rfa.fromlenaddr = NULL;

	return (sys_recvfrom(td, &rfa));
}

/* 
 * XXX This isn't necessary, but it's handy for inserting debug code into
 * sendto().  Let's leave it here for now...
 */	
int
svr4_sys_sendto(td, uap)
        struct thread *td;
        struct svr4_sys_sendto_args *uap;
{
        struct sendto_args sa;

	sa.s = uap->s;
	sa.buf = uap->buf;
	sa.len = uap->len;
	sa.flags = uap->flags;
	sa.to = (caddr_t)uap->to;
	sa.tolen = uap->tolen;

	DPRINTF(("calling sendto()\n"));
	return sys_sendto(td, &sa);
}

