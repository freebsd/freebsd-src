/*
 * Coordination of operations between processes
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"
#include <sys/un.h>
#include <fcntl.h>
#include <dirent.h>

#include "utils/common.h"
#include "utils/eloop.h"
#include "utils/list.h"
#include "proc_coord.h"


struct proc_coord_msg_header {
	u32 msg_type; /* enum proc_coord_message_types */
	u32 cmd; /* proc_coord_commands */
	u32 seq;
};

enum proc_coord_peer_state {
	PROC_COORD_PEER_WAITING,
	PROC_COORD_PEER_ACTIVE,
	PROC_COORD_PEER_TIMED_OUT,
};

struct proc_coord_peer {
	struct dl_list list;
	pid_t pid;
	enum proc_coord_peer_state state;
	struct os_reltime last_rx;
};

struct proc_coord_pending_request {
	struct dl_list list;
	struct proc_coord_peer *peer;
	enum proc_coord_commands cmd;
	u32 seq;
	proc_coord_response_cb cb;
	void *cb_ctx;
	struct os_reltime timeout;
};

struct proc_coord_handler {
	struct dl_list list;
	proc_coord_cb cb;
	void *cb_ctx;
};

struct proc_coord {
	pid_t pid;
	char *dir;
	char *own_sock;
	int sock;
	u32 next_seq;
	struct dl_list handlers; /* struct proc_coord_handler::list */
	struct dl_list peers; /* struct proc_coord_peer::list */
	struct dl_list requests; /* struct proc_coord_pending_request::list */
};


static void proc_coord_set_req_expire_timer(struct proc_coord *pc);


static struct proc_coord_peer * proc_coord_get_peer(struct proc_coord *pc,
						    pid_t pid)
{
	struct proc_coord_peer *peer;

	dl_list_for_each(peer, &pc->peers, struct proc_coord_peer, list) {
		if (peer->pid == pid)
			return peer;
	}

	return NULL;
}


static struct proc_coord_peer * proc_coord_add_peer(struct proc_coord *pc,
						    pid_t pid)
{
	struct proc_coord_peer *peer;

	peer = os_zalloc(sizeof(*peer));
	if (!peer)
		return NULL;

	peer->pid = pid;
	peer->state = PROC_COORD_PEER_WAITING;
	dl_list_add(&pc->peers, &peer->list);

	return peer;
}


static void proc_coord_remove_req(struct proc_coord_pending_request *req)
{
	dl_list_del(&req->list);
	if (req->cb)
		req->cb(req->cb_ctx, req->peer->pid, NULL);
	os_free(req);
}


static void proc_coord_remove_peer(struct proc_coord *pc,
				   struct proc_coord_peer *peer)
{
	struct proc_coord_pending_request *req, *tmp;

	dl_list_for_each_safe(req, tmp, &pc->requests,
			      struct proc_coord_pending_request, list) {
		if (req->peer == peer)
			proc_coord_remove_req(req);
	}

	dl_list_del(&peer->list);
	os_free(peer);
}


static void proc_coord_expire_requests(void *eloop_ctx, void *timeout_ctx)
{
	struct proc_coord *pc = eloop_ctx;
	struct proc_coord_pending_request *req, *tmp;
	struct os_reltime now;

	os_get_reltime(&now);
	dl_list_for_each_safe(req, tmp, &pc->requests,
			      struct proc_coord_pending_request, list) {
		if (os_reltime_before(&req->timeout, &now)) {
			wpa_printf(MSG_DEBUG,
				   "proc_coord: Pending request peer=%u cmd=%d seq=%u timed out",
				   req->peer->pid, req->cmd, req->seq);
			proc_coord_remove_req(req);
		}
	}

	proc_coord_set_req_expire_timer(pc);
}


void proc_coord_cancel_wait(struct proc_coord *pc, proc_coord_response_cb cb,
			    void *cb_ctx)
{
	struct proc_coord_pending_request *req, *tmp;

	dl_list_for_each_safe(req, tmp, &pc->requests,
			      struct proc_coord_pending_request, list) {
		if (req->cb == cb && req->cb_ctx == cb_ctx) {
			req->cb = NULL;
			req->cb_ctx = NULL;
			proc_coord_remove_req(req);
		}
	}

	proc_coord_set_req_expire_timer(pc);
}


static void proc_coord_set_req_expire_timer(struct proc_coord *pc)
{
	struct proc_coord_pending_request *req;
	struct os_reltime *first = NULL;

	eloop_cancel_timeout(proc_coord_expire_requests, pc, NULL);
	dl_list_for_each(req, &pc->requests, struct proc_coord_pending_request,
			 list) {
		if (!first || os_reltime_before(&req->timeout, first))
			first = &req->timeout;
	}

	if (first) {
		struct os_reltime now, res;
		unsigned int ms;

		os_get_reltime(&now);
		if (os_reltime_before(first, &now)) {
			ms = 0;
		} else {
			os_reltime_sub(first, &now, &res);
			ms = os_reltime_in_ms(&res);
		}
		eloop_register_timeout(ms / 1000, (ms % 1000) * 1000,
				       proc_coord_expire_requests, pc, NULL);
	}
}


static struct proc_coord_pending_request *
proc_coord_get_request(struct proc_coord *pc, struct proc_coord_peer *peer,
		       enum proc_coord_commands cmd, u32 seq)
{
	struct proc_coord_pending_request *req;

	dl_list_for_each(req, &pc->requests, struct proc_coord_pending_request,
			 list) {
		if (req->peer == peer && req->cmd == cmd && req->seq == seq)
			return req;
	}

	return NULL;
}


static int proc_coord_send_msg(struct proc_coord *pc,
			       struct proc_coord_peer *peer,
			       enum proc_coord_message_types msg_type,
			       enum proc_coord_commands cmd,
			       u32 seq, const struct wpabuf *msg)
{
	struct proc_coord_msg_header hdr;
	ssize_t res;
	struct sockaddr_un addr;
	struct msghdr mh;
	struct iovec io[2];

	wpa_printf(MSG_DEBUG,
		   "proc_coord: Send message to %d (msg_type=%u cmd=%u seq=%u)",
		   peer->pid, msg_type, cmd, seq);

	os_memset(&hdr, 0, sizeof(hdr));
	hdr.msg_type = msg_type;
	hdr.cmd = cmd;
	hdr.seq = seq;

	os_memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	os_snprintf(addr.sun_path, sizeof(addr.sun_path), "%s/%u",
		    pc->dir, peer->pid);

	io[0].iov_base = &hdr;
	io[0].iov_len = sizeof(hdr);
	if (msg) {
		io[1].iov_base = (void *) wpabuf_head(msg);
		io[1].iov_len = wpabuf_len(msg);
	}

	os_memset(&mh, 0, sizeof(mh));
	mh.msg_iov = io;
	mh.msg_iovlen = msg ? 2 : 1;
	mh.msg_name = (void *) &addr;
	mh.msg_namelen = sizeof(addr);

	res = sendmsg(pc->sock, &mh, MSG_DONTWAIT);
	if (res < 0) {
		int err = errno;

		wpa_printf(MSG_INFO, "proc_coord: sendmsg: %s",
			   strerror(errno));
		if (err == ENOENT || err == ECONNREFUSED) {
			wpa_printf(MSG_INFO,
				   "proc_coord: Remove peer %u due to connection being refused",
				   peer->pid);
			proc_coord_remove_peer(pc, peer);
		}
		return -1;
	}
	return 0;
}


static void proc_coord_rx_starting(struct proc_coord *pc, pid_t pid)
{
	wpa_printf(MSG_DEBUG, "proc_coord: Peer %u STARTING", pid);
}


static void proc_coord_rx_stopping(struct proc_coord *pc, pid_t pid)
{
	struct proc_coord_peer *peer;

	wpa_printf(MSG_DEBUG, "proc_coord: Peer %u STOPPING", pid);

	peer = proc_coord_get_peer(pc, pid);
	if (peer) {
		wpa_printf(MSG_INFO,
			   "proc_coord: Remove peer %u due to STOPPING event",
			   pid);
		proc_coord_remove_peer(pc, peer);
	}
}


static void proc_coord_rx_ping(struct proc_coord *pc, pid_t pid,
			       enum proc_coord_message_types msg_type, u32 seq)
{
	if (msg_type != PROC_COORD_MSG_REQUEST)
		return;

	wpa_printf(MSG_DEBUG, "proc_coord: Reply to peer %u PING", pid);
	proc_coord_send_response(pc, pid, PROC_COORD_CMD_PING, seq, NULL);
}


static void proc_coord_receive(int sock, void *eloop_ctx, void *sock_ctx)
{
	struct proc_coord *pc = eloop_ctx;
	struct sockaddr_un from;
	struct wpabuf *msg;
	size_t msg_len;
	ssize_t res;
	struct proc_coord_msg_header hdr;
	pid_t pid;
	char tmp[20], *pos, *end;
	struct proc_coord_peer *peer;
	struct msghdr mh;
	struct iovec io[2];
	struct proc_coord_handler *handler, *h_tmp;

	res = recv(sock, tmp, 0, MSG_PEEK | MSG_TRUNC);
	if (res < 0) {
		wpa_printf(MSG_ERROR, "proc_coord: recv: %s",
			   strerror(errno));
		return;
	}
	msg_len = res;
	if (msg_len < sizeof(hdr))
		return;
	msg_len -= sizeof(hdr);

	msg = wpabuf_alloc(msg_len);
	if (!msg)
		return;

	io[0].iov_base = &hdr;
	io[0].iov_len = sizeof(hdr);
	io[1].iov_base = wpabuf_mhead(msg);
	io[1].iov_len = msg_len;

	os_memset(&mh, 0, sizeof(mh));
	mh.msg_iov = io;
	mh.msg_iovlen = 2;
	mh.msg_name = (void *) &from;
	mh.msg_namelen = sizeof(from);

	res = recvmsg(sock, &mh, MSG_DONTWAIT);
	if (res < 0) {
		wpa_printf(MSG_ERROR, "proc_coord: recvmsg: %s",
			   strerror(errno));
		goto out;
	}
	if ((size_t) res < sizeof(hdr))
		goto out;

	wpa_printf(MSG_DEBUG, "proc_coord: Received message from %s",
		   from.sun_path);
	if ((size_t) res > sizeof(hdr)) {
		wpabuf_put(msg, res - sizeof(hdr));
		wpa_hexdump_buf(MSG_MSGDUMP,
				"proc_coord: Received message payload", msg);
	}

	end = ((char *) &from) + mh.msg_namelen;

	/* Require same directory for client socket */
	if ((size_t) (end - from.sun_path) < os_strlen(pc->dir) ||
	    os_strncmp(pc->dir, from.sun_path, os_strlen(pc->dir)) != 0)
		goto out;

	/* Find the peer PID from the socket name */
	pos = end - 1;
	while (pos > from.sun_path) {
		if (*pos == '/')
			break;
		pos--;
	}
	if (pos == from.sun_path)
		goto out;
	pos++;
	os_memcpy(tmp, pos, end - pos);
	pid = atoi(tmp);

	wpa_printf(MSG_DEBUG, "proc_coord: pid=%u msg_type=%u cmd=%u seq=%u",
		   pid, hdr.msg_type, hdr.cmd, hdr.seq);

	peer = proc_coord_get_peer(pc, pid);

	switch (hdr.msg_type) {
	case PROC_COORD_MSG_REQUEST:
		break;
	case PROC_COORD_MSG_RESPONSE: {
		struct proc_coord_pending_request *req;

		if (!peer) {
			wpa_printf(MSG_DEBUG,
				   "proc_coord: Discard msg_type=RESPONSE from %u since there is no peer entry for it",
				   pid);
			goto out;
		}

		req = proc_coord_get_request(pc, peer, hdr.cmd, hdr.seq);
		if (!req) {
			wpa_printf(MSG_DEBUG,
				   "proc_coord: Discard msg_type=RESPONSE from %u since there is no pending request for it",
				   pid);
			goto out;
		}
		if (req->cb)
			req->cb(req->cb_ctx, peer->pid, msg);
		dl_list_del(&req->list);
		os_free(req);
		break;
	}
	case PROC_COORD_MSG_EVENT:
		break;
	default:
		wpa_printf(MSG_DEBUG,
			   "proc_coord: Discard unknown msg_type=%u from %u",
			   hdr.msg_type, pid);
		goto out;
	}

	if (!peer) {
		wpa_printf(MSG_DEBUG,
			   "proc_coord: Add new peer entry for %d based on received message",
			   pid);
		peer = proc_coord_add_peer(pc, pid);
		if (!peer) {
			wpa_printf(MSG_ERROR,
				   "proc_coord: Could not add peer entry for %u",
				   pid);
			goto out;
		}
	}

	if (peer->state != PROC_COORD_PEER_ACTIVE) {
		wpa_printf(MSG_DEBUG,
			   "proc_coord: Mark peer %d active due to received message",
			   pid);
		peer->state = PROC_COORD_PEER_ACTIVE;
	}

	os_get_reltime(&peer->last_rx);

	switch (hdr.cmd) {
	case PROC_COORD_CMD_STARTING :
		proc_coord_rx_starting(pc, pid);
		break;
	case PROC_COORD_CMD_STOPPING:
		proc_coord_rx_stopping(pc, pid);
		break;
	case PROC_COORD_CMD_PING:
		proc_coord_rx_ping(pc, pid, hdr.msg_type, hdr.seq);
		break;
	default:
		if (hdr.msg_type == PROC_COORD_MSG_RESPONSE)
			break;

		dl_list_for_each_safe(handler, h_tmp, &pc->handlers,
				 struct proc_coord_handler, list) {
			if (handler->cb(handler->cb_ctx, pid, hdr.msg_type,
					hdr.cmd, hdr.seq, msg))
				break;
		}
		break;
	}

out:
	wpabuf_free(msg);
}


static void proc_coord_send_starting(struct proc_coord *pc)
{
	struct proc_coord_peer *peer, *tmp;
	int count = 0;

	if (dl_list_empty(&pc->peers))
		return;

	wpa_printf(MSG_DEBUG, "proc_coord: Send STARTING event to all peers");
	dl_list_for_each_safe(peer, tmp, &pc->peers, struct proc_coord_peer,
			      list) {
		if (proc_coord_send_msg(pc, peer, PROC_COORD_MSG_EVENT,
					PROC_COORD_CMD_STARTING, 0, NULL) == 0)
			count++;
	}
	wpa_printf(MSG_DEBUG, "proc_coord: STARTING sent to %d peer(s)", count);
}


static void proc_coord_send_stopping(struct proc_coord *pc)
{
	if (dl_list_empty(&pc->peers))
		return;

	wpa_printf(MSG_DEBUG,
		   "proc_coord: Send STOPPING event to all active peers");
	proc_coord_send_event(pc, 0, PROC_COORD_CMD_STOPPING, NULL);
}


static void proc_coord_cb_ping(void *ctx, int pid, const struct wpabuf *msg)
{
	struct proc_coord *pc = ctx;
	struct proc_coord_peer *peer;

	peer = proc_coord_get_peer(pc, pid);
	if (!peer)
		return;
	if (msg) {
		if (peer->state != PROC_COORD_PEER_ACTIVE) {
			wpa_printf(MSG_DEBUG,
				   "proc_coord: Mark peer %d active due to response to PING",
				   pid);
			peer->state = PROC_COORD_PEER_ACTIVE;
		}
	} else {
		if (peer->state != PROC_COORD_PEER_TIMED_OUT) {
			wpa_printf(MSG_DEBUG,
				   "proc_coord: Mark peer %d timed out due to no response to PING",
				   pid);
			peer->state = PROC_COORD_PEER_TIMED_OUT;
		}
	}
}


static void proc_coord_update_peers_from_dir(struct proc_coord *pc)
{
	DIR *dir;
	struct dirent *de;
	struct proc_coord_peer *peer, *tmp;

	/* Remove peers that do not have a socket file */
	dl_list_for_each_safe(peer, tmp, &pc->peers, struct proc_coord_peer,
			      list) {
		char fname[256];

		os_snprintf(fname, sizeof(fname), "%s/%d", pc->dir, peer->pid);
		if (!os_file_exists(fname)) {
			wpa_printf(MSG_DEBUG,
				   "proc_coord: Remove peer %d due to socket file not present",
				   peer->pid);
			proc_coord_remove_peer(pc, peer);
		}
	}

	/* Add peer entries for all new sockets in the directory */
	dir = opendir(pc->dir);
	if (!dir)  {
		wpa_printf(MSG_ERROR, "proc_coord: opendir: %s",
			   strerror(errno));
		return;
	}

	while ((de = readdir(dir))) {
		int pid = atoi(de->d_name);

		if (pid <= 0 || pid == pc->pid)
			continue;

		peer = proc_coord_get_peer(pc, pid);
		if (peer)
			continue;

		wpa_printf(MSG_DEBUG, "proc_coord: Add new peer entry for %u",
			   pid);
		peer = proc_coord_add_peer(pc, pid);
		if (!peer) {
			wpa_printf(MSG_ERROR,
				   "proc_coord: Could not add peer entry for %u",
				   pid);
			continue;
		}

		wpa_printf(MSG_DEBUG, "proc_coord: Ping new peer %u", pid);
		proc_coord_send_request(pc, pid, PROC_COORD_CMD_PING, NULL,
					10000, proc_coord_cb_ping, pc);
	}

	closedir(dir);
}


static void proc_coord_update_peers(void *eloop_ctx, void *timeout_ctx)
{
	struct proc_coord *pc = eloop_ctx;
	struct proc_coord_peer *peer, *tmp;
	struct os_reltime now;

	proc_coord_update_peers_from_dir(pc);

	os_get_reltime(&now);
	dl_list_for_each_safe(peer, tmp, &pc->peers, struct proc_coord_peer,
			      list) {
		if (!os_reltime_expired(&now, &peer->last_rx, 60))
			continue;
		proc_coord_send_request(pc, peer->pid, PROC_COORD_CMD_PING,
					NULL, 10000, NULL, NULL);
	}

	eloop_register_timeout(10, 0, proc_coord_update_peers, pc, NULL);
}


struct proc_coord * proc_coord_init(const char *dir)
{
	struct proc_coord *pc;
	struct sockaddr_un addr;
	size_t len;
	int flags;

	pc = os_zalloc(sizeof(*pc));
	if (!pc)
		return NULL;

	dl_list_init(&pc->peers);
	dl_list_init(&pc->requests);
	dl_list_init(&pc->handlers);
	pc->sock = -1;

	pc->dir = os_strdup(dir);
	if (!pc->dir)
		goto fail;

	len = os_strlen(dir) + 20;
	pc->own_sock = os_zalloc(len);
	if (!pc->own_sock)
		goto fail;
	pc->pid = getpid();
	os_snprintf(pc->own_sock, len, "%s/%d", dir, pc->pid);
	wpa_printf(MSG_DEBUG, "proc_coord: Own socket at %s", pc->own_sock);
	unlink(pc->own_sock);

	pc->sock = socket(PF_UNIX, SOCK_DGRAM, 0);
	if (pc->sock < 0) {
		wpa_printf(MSG_ERROR, "proc_coord: socket(PF_UNIX): %s",
			   strerror(errno));
		goto fail;
	}

	os_memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	os_strlcpy(addr.sun_path, pc->own_sock, sizeof(addr.sun_path));
	if (bind(pc->sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		wpa_printf(MSG_ERROR, "proc_coord: bind(PF_UNIX) failed: %s",
			   strerror(errno));
		goto fail;
	}

	flags = fcntl(pc->sock, F_GETFL);
	if (flags >= 0) {
		flags |= O_NONBLOCK;
		if (fcntl(pc->sock, F_SETFL, flags) < 0) {
			wpa_printf(MSG_INFO,
				   "proc_coord: fcntl(O_NONBLOCK): %s",
				   strerror(errno));
			/* Not fatal, continue on.*/
		}
	}

	eloop_register_read_sock(pc->sock, proc_coord_receive, pc, NULL);

	proc_coord_update_peers_from_dir(pc);

	/* Start periodic updates quickly to recover from potential race
	 * conditions if the processes are started at the same time. */
	eloop_register_timeout(1, 0, proc_coord_update_peers, pc, NULL);

	proc_coord_send_starting(pc);

	return pc;

fail:
	proc_coord_deinit(pc);
	return NULL;
}


void proc_coord_deinit(struct proc_coord *pc)
{
	struct proc_coord_peer *peer, *tmp;
	struct proc_coord_pending_request *req, *tmp2;

	if (!pc)
		return;

	eloop_cancel_timeout(proc_coord_update_peers, pc, NULL);
	eloop_cancel_timeout(proc_coord_expire_requests, pc, NULL);

	if (pc->sock >= 0) {
		proc_coord_send_stopping(pc);
		eloop_unregister_read_sock(pc->sock);
		close(pc->sock);
		unlink(pc->own_sock);
	}

	dl_list_for_each_safe(peer, tmp, &pc->peers, struct proc_coord_peer,
			      list)
		proc_coord_remove_peer(pc, peer);

	dl_list_for_each_safe(req, tmp2, &pc->requests,
			      struct proc_coord_pending_request, list) {
		dl_list_del(&req->list);
		os_free(req);
	}

	os_free(pc->dir);
	os_free(pc->own_sock);
	os_free(pc);
}


int proc_coord_register_handler(struct proc_coord *pc, proc_coord_cb cb,
				void *cb_ctx)
{
	struct proc_coord_handler *handler;

	handler = os_zalloc(sizeof(*handler));
	if (!handler)
		return -1;

	handler->cb = cb;
	handler->cb_ctx = cb_ctx;
	dl_list_add(&pc->handlers, &handler->list);
	return 0;
}


void proc_coord_unregister_handler(struct proc_coord *pc, proc_coord_cb cb,
				   void *cb_ctx)
{
	struct proc_coord_handler *handler;

	dl_list_for_each(handler, &pc->handlers, struct proc_coord_handler,
			 list) {
		if (handler->cb == cb && handler->cb_ctx == cb_ctx) {
			dl_list_del(&handler->list);
			os_free(handler);
			break;
		}
	}
}


int proc_coord_send_event(struct proc_coord *pc, int dst,
			  enum proc_coord_commands cmd,
			  const struct wpabuf *msg)
{
	struct proc_coord_peer *peer, *tmp;
	int count = 0;
	u32 seq = 0;

	dl_list_for_each_safe(peer, tmp, &pc->peers, struct proc_coord_peer,
			      list) {
		if (dst && peer->pid != dst)
			continue;
		if (!dst && peer->state != PROC_COORD_PEER_ACTIVE)
			continue;
		if (proc_coord_send_msg(pc, peer, PROC_COORD_MSG_EVENT, cmd,
					seq, msg) == 0)
			count++;
	}

	return count;
}


int proc_coord_send_request(struct proc_coord *pc, int dst,
			    enum proc_coord_commands cmd,
			    const struct wpabuf *msg,
			    unsigned int timeout_ms,
			    proc_coord_response_cb cb,
			    void *cb_ctx)
{
	struct proc_coord_peer *peer, *tmp;
	int count = 0;
	struct proc_coord_pending_request *req;

	pc->next_seq++;

	dl_list_for_each_safe(peer, tmp, &pc->peers, struct proc_coord_peer,
			      list) {
		if (dst && peer->pid != dst)
			continue;
		if (!dst && peer->state != PROC_COORD_PEER_ACTIVE)
			continue;
		if (proc_coord_send_msg(pc, peer, PROC_COORD_MSG_REQUEST, cmd,
					pc->next_seq, msg) < 0)
			continue;
		count++;

		req = os_zalloc(sizeof(*req));
		if (!req)
			break;
		req->peer = peer;
		req->cmd = cmd;
		req->seq = pc->next_seq;
		req->cb = cb;
		req->cb_ctx = cb_ctx;
		os_get_reltime(&req->timeout);
		os_reltime_add_ms(&req->timeout, timeout_ms);
		dl_list_add(&pc->requests, &req->list);
	}
	proc_coord_set_req_expire_timer(pc);

	return count;
}


int proc_coord_send_response(struct proc_coord *pc, int dst,
			     enum proc_coord_commands cmd, u32 seq,
			     const struct wpabuf *msg)
{
	struct proc_coord_peer *peer = proc_coord_get_peer(pc, dst);

	if (!peer)
		return -1;
	return proc_coord_send_msg(pc, peer, PROC_COORD_MSG_RESPONSE, cmd,
				   seq, msg);
}
