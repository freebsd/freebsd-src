/*
 * Copyright (c) 1997-2005 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden). 
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution. 
 *
 * 3. Neither the name of the Institute nor the names of its contributors 
 *    may be used to endorse or promote products derived from this software 
 *    without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. 
 */

#include "kcm_locl.h"

RCSID("$Id: connect.c 16314 2005-11-29 19:03:50Z lha $");

struct descr {
    int s;
    int type;
    char *path;
    unsigned char *buf;
    size_t size;
    size_t len;
    time_t timeout;
    struct sockaddr_storage __ss;
    struct sockaddr *sa;
    socklen_t sock_len;
    kcm_client peercred;
};

static void
init_descr(struct descr *d)
{
    memset(d, 0, sizeof(*d));
    d->sa = (struct sockaddr *)&d->__ss;
    d->s = -1;
}

/*
 * re-initialize all `n' ->sa in `d'.
 */

static void
reinit_descrs (struct descr *d, int n)
{
    int i;

    for (i = 0; i < n; ++i)
	d[i].sa = (struct sockaddr *)&d[i].__ss;
}

/*
 * Update peer credentials from socket.
 *
 * SCM_CREDS can only be updated the first time there is read data to
 * read from the filedescriptor, so if we read do it before this
 * point, the cred data might not be is not there yet.
 */

static int
update_client_creds(int s, kcm_client *peer)
{
#ifdef GETPEERUCRED
    /* Solaris 10 */
    {
	ucred_t *peercred;
	
	if (getpeerucred(s, &peercred) != 0) {
	    peer->uid = ucred_geteuid(peercred);
	    peer->gid = ucred_getegid(peercred);
	    peer->pid = 0;
	    ucred_free(peercred);
	    return 0;
	}
    } 
#endif
#ifdef GETPEEREID
    /* FreeBSD, OpenBSD */
    {
	uid_t uid;
	gid_t gid;

	if (getpeereid(s, &uid, &gid) == 0) {
	    peer->uid = uid;
	    peer->gid = gid;
	    peer->pid = 0;
	    return 0;
	}
    }
#endif
#ifdef SO_PEERCRED
    /* Linux */
    {
	struct ucred pc;
	socklen_t pclen = sizeof(pc);

	if (getsockopt(s, SOL_SOCKET, SO_PEERCRED, (void *)&pc, &pclen) == 0) {
	    peer->uid = pc.uid;
	    peer->gid = pc.gid;
	    peer->pid = pc.pid;
	    return 0;
	}
    }
#endif
#if defined(LOCAL_PEERCRED) && defined(XUCRED_VERSION)
    {
	struct xucred peercred;
	socklen_t peercredlen = sizeof(peercred);

	if (getsockopt(s, LOCAL_PEERCRED, 1,
		       (void *)&peercred, &peercredlen) == 0
	    && peercred.cr_version == XUCRED_VERSION)
	{
	    peer->uid = peercred.cr_uid;
	    peer->gid = peercred.cr_gid;
	    peer->pid = 0;
	    return 0;
	}
    }
#endif
#if defined(SOCKCREDSIZE) && defined(SCM_CREDS)
    /* NetBSD */
    if (peer->uid == -1) {
	struct msghdr msg;
	socklen_t crmsgsize;
	void *crmsg;
	struct cmsghdr *cmp;
	struct sockcred *sc;
	
	memset(&msg, 0, sizeof(msg));
	crmsgsize = CMSG_SPACE(SOCKCREDSIZE(NGROUPS));
	if (crmsgsize == 0)
	    return 1 ;

	crmsg = malloc(crmsgsize);
	if (crmsg == NULL)
	    goto failed_scm_creds;

	memset(crmsg, 0, crmsgsize);
	
	msg.msg_control = crmsg;
	msg.msg_controllen = crmsgsize;
	
	if (recvmsg(s, &msg, 0) < 0) {
	    free(crmsg);
	    goto failed_scm_creds;
	}	
	
	if (msg.msg_controllen == 0 || (msg.msg_flags & MSG_CTRUNC) != 0) {
	    free(crmsg);
	    goto failed_scm_creds;
	}	
	
	cmp = CMSG_FIRSTHDR(&msg);
	if (cmp->cmsg_level != SOL_SOCKET || cmp->cmsg_type != SCM_CREDS) {
	    free(crmsg);
	    goto failed_scm_creds;
	}	
	
	sc = (struct sockcred *)(void *)CMSG_DATA(cmp);
	
	peer->uid = sc->sc_euid;
	peer->gid = sc->sc_egid;
	peer->pid = 0;
	
	free(crmsg);
	return 0;
    } else {
	/* we already got the cred, just return it */
	return 0;
    }
 failed_scm_creds:
#endif
    krb5_warn(kcm_context, errno, "failed to determine peer identity");
    return 1;
}


/*
 * Create the socket (family, type, port) in `d'
 */

static void 
init_socket(struct descr *d)
{
    struct sockaddr_un un;
    struct sockaddr *sa = (struct sockaddr *)&un;
    krb5_socklen_t sa_size = sizeof(un);

    init_descr (d);

    un.sun_family = AF_UNIX;

    if (socket_path != NULL)
	d->path = socket_path;
    else
	d->path = _PATH_KCM_SOCKET;

    strlcpy(un.sun_path, d->path, sizeof(un.sun_path));

    d->s = socket(AF_UNIX, SOCK_STREAM, 0);
    if (d->s < 0){
	krb5_warn(kcm_context, errno, "socket(%d, %d, 0)", AF_UNIX, SOCK_STREAM);
	d->s = -1;
	return;
    }
#if defined(HAVE_SETSOCKOPT) && defined(SOL_SOCKET) && defined(SO_REUSEADDR)
    {
	int one = 1;
	setsockopt(d->s, SOL_SOCKET, SO_REUSEADDR, (void *)&one, sizeof(one));
    }
#endif
#ifdef LOCAL_CREDS
    {
	int one = 1;
	setsockopt(d->s, 0, LOCAL_CREDS, (void *)&one, sizeof(one));
    }
#endif

    d->type = SOCK_STREAM;

    unlink(d->path);

    if (bind(d->s, sa, sa_size) < 0) {
	krb5_warn(kcm_context, errno, "bind %s", un.sun_path);
	close(d->s);
	d->s = -1;
	return;
    }

    if (listen(d->s, SOMAXCONN) < 0) {
	krb5_warn(kcm_context, errno, "listen %s", un.sun_path);
	close(d->s);
	d->s = -1;
	return;
    }

    chmod(d->path, 0777);

    return;
}

/*
 * Allocate descriptors for all the sockets that we should listen on
 * and return the number of them.
 */

static int
init_sockets(struct descr **desc)
{
    struct descr *d;
    size_t num = 0;

    d = (struct descr *)malloc(sizeof(*d));
    if (d == NULL) {
	krb5_errx(kcm_context, 1, "malloc failed");
    }

    init_socket(d);
    if (d->s != -1) {
	kcm_log(5, "listening on domain socket %s", d->path);
	num++;
    }

    reinit_descrs (d, num);
    *desc = d;

    return num;
}

/*
 * handle the request in `buf, len', from `addr' (or `from' as a string),
 * sending a reply in `reply'.
 */

static int
process_request(unsigned char *buf, 
		size_t len, 
		krb5_data *reply,
		kcm_client *client)
{
    krb5_data request;
   
    if (len < 4) {
	kcm_log(1, "malformed request from process %d (too short)", 
		client->pid);
	return -1;
    }

    if (buf[0] != KCM_PROTOCOL_VERSION_MAJOR ||
	buf[1] != KCM_PROTOCOL_VERSION_MINOR) {
	kcm_log(1, "incorrect protocol version %d.%d from process %d",
		buf[0], buf[1], client->pid);
	return -1;
    }

    buf += 2;
    len -= 2;

    /* buf is now pointing at opcode */

    request.data = buf;
    request.length = len;

    return kcm_dispatch(kcm_context, client, &request, reply);
}

/*
 * Handle the request in `buf, len' to socket `d'
 */

static void
do_request(void *buf, size_t len, struct descr *d)
{
    krb5_error_code ret;
    krb5_data reply;

    reply.length = 0;

    ret = process_request(buf, len, &reply, &d->peercred);
    if (reply.length != 0) {
	unsigned char len[4];
	struct msghdr msghdr;
	struct iovec iov[2];

	kcm_log(5, "sending %lu bytes to process %d", 
		(unsigned long)reply.length,
		(int)d->peercred.pid);

	memset (&msghdr, 0, sizeof(msghdr));
	msghdr.msg_name       = NULL;
	msghdr.msg_namelen    = 0;
	msghdr.msg_iov        = iov;
	msghdr.msg_iovlen     = sizeof(iov)/sizeof(*iov);
#if 0
	msghdr.msg_control    = NULL;
	msghdr.msg_controllen = 0;
#endif

	len[0] = (reply.length >> 24) & 0xff;
	len[1] = (reply.length >> 16) & 0xff;
	len[2] = (reply.length >> 8) & 0xff;
	len[3] = reply.length & 0xff;

	iov[0].iov_base       = (void*)len;
	iov[0].iov_len        = 4;
	iov[1].iov_base       = reply.data;
	iov[1].iov_len        = reply.length;

	if (sendmsg (d->s, &msghdr, 0) < 0) {
	    kcm_log (0, "sendmsg(%d): %d %s", (int)d->peercred.pid,
		     errno, strerror(errno));
	    krb5_data_free(&reply);
	    return;
	}

	krb5_data_free(&reply);
    }

    if (ret) {
	kcm_log(0, "Failed processing %lu byte request from process %d", 
		(unsigned long)len, d->peercred.pid);
    }
}

static void
clear_descr(struct descr *d)
{
    if(d->buf)
	memset(d->buf, 0, d->size);
    d->len = 0;
    if(d->s != -1)
	close(d->s);
    d->s = -1;
}

#define STREAM_TIMEOUT 4

/*
 * accept a new stream connection on `d[parent]' and store it in `d[child]'
 */

static void
add_new_stream (struct descr *d, int parent, int child)
{
    int s;

    if (child == -1)
	return;

    d[child].peercred.pid = -1;
    d[child].peercred.uid = -1;
    d[child].peercred.gid = -1;

    d[child].sock_len = sizeof(d[child].__ss);
    s = accept(d[parent].s, d[child].sa, &d[child].sock_len);
    if(s < 0) {
	krb5_warn(kcm_context, errno, "accept");
	return;
    }

    if (s >= FD_SETSIZE) {
	krb5_warnx(kcm_context, "socket FD too large");
	close (s);
	return;
    }

    d[child].s = s;
    d[child].timeout = time(NULL) + STREAM_TIMEOUT;
    d[child].type = SOCK_STREAM;
}

/*
 * Grow `d' to handle at least `n'.
 * Return != 0 if fails
 */

static int
grow_descr (struct descr *d, size_t n)
{
    if (d->size - d->len < n) {
	unsigned char *tmp;
	size_t grow;

	grow = max(1024, d->len + n);
	if (d->size + grow > max_request) {
	    kcm_log(0, "Request exceeds max request size (%lu bytes).",
		    (unsigned long)d->size + grow);
	    clear_descr(d);
	    return -1;
	}
	tmp = realloc (d->buf, d->size + grow);
	if (tmp == NULL) {
	    kcm_log(0, "Failed to re-allocate %lu bytes.",
		    (unsigned long)d->size + grow);
	    clear_descr(d);
	    return -1;
	}
	d->size += grow;
	d->buf = tmp;
    }
    return 0;
}

/*
 * Handle incoming data to the stream socket in `d[index]'
 */

static void
handle_stream(struct descr *d, int index, int min_free)
{
    unsigned char buf[1024];
    int n;
    int ret = 0;

    if (d[index].timeout == 0) {
	add_new_stream (d, index, min_free);
	return;
    }

    if (update_client_creds(d[index].s, &d[index].peercred)) {
	krb5_warnx(kcm_context, "failed to update peer identity");
	clear_descr(d + index);
	return;
    }

    if (d[index].peercred.uid == -1) {
	krb5_warnx(kcm_context, "failed to determine peer identity");
	clear_descr (d + index);
	return;
    }

    n = recvfrom(d[index].s, buf, sizeof(buf), 0, NULL, NULL);
    if (n < 0) {
	krb5_warn(kcm_context, errno, "recvfrom");
	return;
    } else if (n == 0) {
	krb5_warnx(kcm_context, "connection closed before end of data "
		   "after %lu bytes from process %ld",
		   (unsigned long) d[index].len, (long) d[index].peercred.pid);
	clear_descr (d + index);
	return;
    }
    if (grow_descr (&d[index], n))
	return;
    memcpy(d[index].buf + d[index].len, buf, n);
    d[index].len += n;
    if (d[index].len > 4) {
	krb5_storage *sp;
	int32_t len;

	sp = krb5_storage_from_mem(d[index].buf, d[index].len);
	if (sp == NULL) {
	    kcm_log (0, "krb5_storage_from_mem failed");
	    ret = -1;
	} else {
	    krb5_ret_int32(sp, &len);
	    krb5_storage_free(sp);
	    if (d[index].len - 4 >= len) {
		memmove(d[index].buf, d[index].buf + 4, d[index].len - 4);
		ret = 1;
	    } else
		ret = 0;
	}
    }
    if (ret < 0)
	return;
    else if (ret == 1) {
	do_request(d[index].buf, d[index].len, &d[index]);
	clear_descr(d + index);
    }
}

#ifdef HAVE_DOOR_CREATE

static void
kcm_door_server(void  *cookie, char *argp, size_t arg_size,
		door_desc_t *dp, uint_t n_desc)
{
    kcm_client peercred;
    door_cred_t cred;
    krb5_error_code ret;
    krb5_data reply;
    size_t length;
    char *p;

    reply.length = 0;

    p = NULL;
    length = 0;

    if (door_cred(&cred) != 0) {
	kcm_log(0, "door_cred failed with %s", strerror(errno));
	goto out;
    }

    peercred.uid = cred.dc_euid;
    peercred.gid = cred.dc_egid;
    peercred.pid = cred.dc_pid;

    ret = process_request((unsigned char*)argp, arg_size, &reply, &peercred);
    if (reply.length != 0) {
	p = alloca(reply.length); /* XXX don't use alloca */
	if (p) {
	    memcpy(p, reply.data, reply.length);
	    length = reply.length;
	}
	krb5_data_free(&reply);
    }

 out:
    door_return(p, length, NULL, 0);
}

static void
kcm_setup_door(void)
{
    int fd, ret;
    char *path;

    fd = door_create(kcm_door_server, NULL, 0);
    if (fd < 0)
	krb5_err(kcm_context, 1, errno, "Failed to create door");

    if (door_path != NULL)
	path = door_path;
    else
	path = _PATH_KCM_DOOR;

    unlink(path);
    ret = open(path, O_RDWR | O_CREAT, 0666);
    if (ret < 0)
	krb5_err(kcm_context, 1, errno, "Failed to create/open door");
    close(ret);

    ret = fattach(fd, path);
    if (ret < 0)
	krb5_err(kcm_context, 1, errno, "Failed to attach door");

}
#endif /* HAVE_DOOR_CREATE */


void
kcm_loop(void)
{
    struct descr *d;
    int ndescr;

#ifdef HAVE_DOOR_CREATE
    kcm_setup_door();
#endif

    ndescr = init_sockets(&d);
    if (ndescr <= 0) {
	krb5_warnx(kcm_context, "No sockets!");
#ifndef HAVE_DOOR_CREATE
	exit(1);
#endif
    }
    while (exit_flag == 0){
	struct timeval tmout;
	fd_set fds;
	int min_free = -1;
	int max_fd = 0;
	int i;

	FD_ZERO(&fds);
	for(i = 0; i < ndescr; i++) {
	    if (d[i].s >= 0){
		if(d[i].type == SOCK_STREAM && 
		   d[i].timeout && d[i].timeout < time(NULL)) {
		    kcm_log(1, "Stream connection from %d expired after %lu bytes",
			    d[i].peercred.pid, (unsigned long)d[i].len);
		    clear_descr(&d[i]);
		    continue;
		}
		if (max_fd < d[i].s)
		    max_fd = d[i].s;
		if (max_fd >= FD_SETSIZE)
		    krb5_errx(kcm_context, 1, "fd too large");
		FD_SET(d[i].s, &fds);
	    } else if (min_free < 0 || i < min_free)
		min_free = i;
	}
	if (min_free == -1) {
	    struct descr *tmp;
	    tmp = realloc(d, (ndescr + 4) * sizeof(*d));
	    if(tmp == NULL)
		krb5_warnx(kcm_context, "No memory");
	    else {
		d = tmp;
		reinit_descrs (d, ndescr);
		memset(d + ndescr, 0, 4 * sizeof(*d));
		for(i = ndescr; i < ndescr + 4; i++)
		    init_descr (&d[i]);
		min_free = ndescr;
		ndescr += 4;
	    }
	}

	tmout.tv_sec = STREAM_TIMEOUT;
	tmout.tv_usec = 0;
	switch (select(max_fd + 1, &fds, 0, 0, &tmout)){
	case 0:
	    kcm_run_events(kcm_context, time(NULL));
	    break;
	case -1:
	    if (errno != EINTR)
		krb5_warn(kcm_context, errno, "select");
	    break;
	default:
	    for(i = 0; i < ndescr; i++) {
		if(d[i].s >= 0 && FD_ISSET(d[i].s, &fds)) {
		    if (d[i].type == SOCK_STREAM)
			handle_stream(d, i, min_free);
		}
	    }
	    kcm_run_events(kcm_context, time(NULL));
	    break;
	}
    }
    if (d->path != NULL)
	unlink(d->path);
    free(d);
}

