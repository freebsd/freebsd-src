/*-
 * Copyright (c) 2017 Juniper Networks, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *	notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *	notice, this list of conditions and the following disclaimer in the
 *	documentation and/or other materials provided with the distribution.
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
 *
 */

/*
 * This file contains 9P client functions which prepares message to be sent to
 * the server. Every fileop typically has a function defined here to interact
 * with the host.
 */

#include <vm/uma.h>
#include <sys/systm.h>
#include <sys/dirent.h>
#include <sys/fcntl.h>
#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/sysctl.h>

#include <fs/p9fs/p9_client.h>
#include <fs/p9fs/p9_debug.h>
#include <fs/p9fs/p9_transport.h>

#define QEMU_HEADER 7
#define P9FS_MAX_FID_CNT (1024 * 1024 * 1024)
#define P9FS_ROOT_FID_NO 2
#define P9FS_MIN_TAG 1
#define P9FS_MAX_TAG 65535
#define WSTAT_SIZE 47
#define WSTAT_EXTENSION_SIZE 14

static MALLOC_DEFINE(M_P9CLNT, "p9_client", "p9fs client structure");
static uma_zone_t p9fs_fid_zone;
static uma_zone_t p9fs_req_zone;
static uma_zone_t p9fs_buf_zone;

SYSCTL_DECL(_vfs_p9fs);
int p9_debug_level = 0;
SYSCTL_INT(_vfs_p9fs, OID_AUTO, debug_level, CTLFLAG_RW,
    &p9_debug_level, 0, "p9fs debug logging");

static struct p9_req_t *p9_get_request(struct p9_client *c, int *error);
static struct p9_req_t *p9_client_request(
    struct p9_client *c, int8_t type, int *error, const char *fmt, ...);

inline int
p9_is_proto_dotl(struct p9_client *clnt)
{

	return (clnt->proto_version == p9_proto_2000L);
}

inline int
p9_is_proto_dotu(struct p9_client *clnt)
{

	return (clnt->proto_version == p9_proto_2000u);
}

/* Parse mount options into client structure */
static int
p9_parse_opts(struct mount  *mp, struct p9_client *clnt)
{
	int error, len;
	char *trans;

	/*
	 * Default to virtio since thats the only transport we have for now.
	 */
	error = vfs_getopt(mp->mnt_optnew, "trans", (void **)&trans, &len);
	if (error == ENOENT)
		trans = "virtio";

	/* These are defaults for now */
	clnt->proto_version = p9_proto_2000L;
	clnt->msize = 8192;

	/* Get the default trans callback */
	clnt->ops = p9_get_trans_by_name(trans);

	return (0);
}

/* Allocate buffer for sending request and getting responses */
static struct p9_buffer *
p9_buffer_alloc(int alloc_msize)
{
	struct p9_buffer *fc;

	fc = uma_zalloc(p9fs_buf_zone, M_WAITOK | M_ZERO);
	fc->capacity = alloc_msize;
	fc->offset = 0;
	fc->size = 0;
	fc->sdata = (char *)fc + sizeof(struct p9_buffer);

	return (fc);
}

/* Free memory used by request and response buffers */
static void
p9_buffer_free(struct p9_buffer **buf)
{

	/* Free the sdata buffers first, then the whole structure*/
	uma_zfree(p9fs_buf_zone, *buf);
	*buf = NULL;
}

/* Free the request */
static void
p9_free_req(struct p9_client *clnt, struct p9_req_t *req)
{

	if (req->tc != NULL) {
		if (req->tc->tag != P9_NOTAG)
			p9_tag_destroy(clnt, req->tc->tag);
		p9_buffer_free(&req->tc);
	}

	if (req->rc != NULL)
		p9_buffer_free(&req->rc);

	uma_zfree(p9fs_req_zone, req);
}

/* Allocate a request by tag */
static struct p9_req_t *
p9_get_request(struct p9_client *clnt, int *error)
{
	struct p9_req_t *req;
	int alloc_msize;
	uint16_t tag;

	alloc_msize = P9FS_MTU;

	req = uma_zalloc(p9fs_req_zone, M_WAITOK | M_ZERO);
	req->tc = p9_buffer_alloc(alloc_msize);
	req->rc = p9_buffer_alloc(alloc_msize);

	tag = p9_tag_create(clnt);
	if (tag == P9_NOTAG) {
		*error = EAGAIN;
		req->tc->tag = P9_NOTAG;
		p9_free_req(clnt, req);
		return (NULL);
	}
	req->tc->tag = tag;
	return (req);
}

/* Parse header arguments of the response buffer */
static int
p9_parse_receive(struct p9_buffer *buf, struct p9_client *clnt)
{
	int8_t type;
	int16_t tag;
	int32_t size;
	int error;

	buf->offset = 0;

	/* This value is set by QEMU for the header.*/
	if (buf->size == 0)
		buf->size = QEMU_HEADER;

	/* This is the initial header. Parse size, type, and tag .*/
	error = p9_buf_readf(buf, 0, "dbw", &size, &type, &tag);
	if (error != 0)
		goto out;

	buf->size = size;
	buf->id = type;
	buf->tag = tag;
	P9_DEBUG(TRANS, "%s: size=%d type: %d tag: %d\n",
	    __func__, buf->size, buf->id, buf->tag);
out:
	return (error);
}

/* Check 9P response for any errors returned and process it */
static int
p9_client_check_return(struct p9_client *c, struct p9_req_t *req)
{
	int error;
	int ecode;
	char *ename;

	/* Check what we have in the receive bufer .*/
	error = p9_parse_receive(req->rc, c);
	if (error != 0)
		goto out;

	/*
	 * No error, We are done with the preprocessing. Return to the caller
	 * and process the actual data.
	 */
	if (req->rc->id != P9PROTO_RERROR && req->rc->id != P9PROTO_RLERROR)
		return (0);

	/*
	 * Interpreting the error is done in different ways for Linux and
	 * Unix version. Make sure you interpret it right.
	 */
	if (req->rc->id == P9PROTO_RERROR) {
	        error = p9_buf_readf(req->rc, c->proto_version, "s?d", &ename, &ecode);
	} else if (req->rc->id == P9PROTO_RLERROR) {
	        error = p9_buf_readf(req->rc, c->proto_version, "d", &ecode);
	} else {
		goto out;
	}
	if (error != 0)
		goto out;

	/* if there was an ecode error make this the err now */
	error = ecode;

	/*
	 * Note this is still not completely an error, as lookups for files
	 * not present can hit this and return. Hence it is made a debug print.
	 */
	if (error != 0) {
	        if (req->rc->id == P9PROTO_RERROR) {
		        P9_DEBUG(PROTO, "RERROR error %d ename %s\n",
			    error, ename);
	        } else if (req->rc->id == P9PROTO_RLERROR) {
		        P9_DEBUG(PROTO, "RLERROR error %d\n", error);
		}
	}

	if (req->rc->id == P9PROTO_RERROR) {
	        free(ename, M_TEMP);
	}
	return (error);

out:
	P9_DEBUG(ERROR, "couldn't parse receive buffer error%d\n", error);
	return (error);
}

/* State machine changing helpers */
void p9_client_disconnect(struct p9_client *clnt)
{

	P9_DEBUG(TRANS, "%s: clnt %p\n", __func__, clnt);
	clnt->trans_status = P9FS_DISCONNECT;
}

void p9_client_begin_disconnect(struct p9_client *clnt)
{

	P9_DEBUG(TRANS, "%s: clnt %p\n", __func__, clnt);
	clnt->trans_status = P9FS_BEGIN_DISCONNECT;
}

static struct p9_req_t *
p9_client_prepare_req(struct p9_client *c, int8_t type,
    int req_size, int *error, const char *fmt, __va_list ap)
{
	struct p9_req_t *req;

	P9_DEBUG(TRANS, "%s: client %p op %d\n", __func__, c, type);

	/*
	 * Before we start with the request, check if its possible to finish
	 * this request. We are allowed to submit the request only if there
	 * are no close sessions happening or else there can be race. If the
	 * status is Disconnected, we stop any requests coming in after that.
	 */
	if (c->trans_status == P9FS_DISCONNECT) {
		*error = EIO;
		return (NULL);
	}

	/* Allow only cleanup clunk messages once teardown has started. */
	if ((c->trans_status == P9FS_BEGIN_DISCONNECT) &&
	    (type != P9PROTO_TCLUNK)) {
		*error = EIO;
		return (NULL);
	}

	/* Allocate buffer for transferring and receiving data from host */
	req = p9_get_request(c, error);
	if (*error != 0) {
		P9_DEBUG(ERROR, "%s: request allocation failed.\n", __func__);
		return (NULL);
	}

	/* Marshall the data according to QEMU standards */
	*error = p9_buf_prepare(req->tc, type);
	if (*error != 0) {
		P9_DEBUG(ERROR, "%s: p9_buf_prepare failed: %d\n",
		    __func__, *error);
		goto out;
	}

	*error = p9_buf_vwritef(req->tc, c->proto_version, fmt, ap);
	if (*error != 0) {
		P9_DEBUG(ERROR, "%s: p9_buf_vwrite failed: %d\n",
		    __func__, *error);
		goto out;
	}

	*error = p9_buf_finalize(c, req->tc);
	if (*error != 0) {
		P9_DEBUG(ERROR, "%s: p9_buf_finalize failed: %d \n",
		    __func__, *error);
		goto out;
	}

	return (req);
out:
	p9_free_req(c, req);
	return (NULL);
}

/*
 * Issue a request and wait for response. The routine takes care of preparing
 * the 9P request header to be sent, parsing and checking for error conditions
 * in the received buffer. It returns the request structure.
 */
static struct p9_req_t *
p9_client_request(struct p9_client *c, int8_t type, int *error,
    const char *fmt, ...)
{
	va_list ap;
	struct p9_req_t *req;

	va_start(ap, fmt);
	req = p9_client_prepare_req(c, type, c->msize, error, fmt, ap);
	va_end(ap);

	/* Issue with allocation of request buffer */
	if (*error != 0)
		return (NULL);

	/* Call into the transport for submission. */
	*error = c->ops->request(c->handle, req);
	if (*error != 0) {
		P9_DEBUG(ERROR, "%s: failed: %d\n", __func__, *error);
		goto out;
	}

	/*
	 * Before we return, pre process the header and the rc buffer before
	 * calling into the protocol infra to analyze the data in rc.
	 */
	*error = p9_client_check_return(c, req);
	if (*error != 0)
		goto out;

	return (req);
out:
	p9_free_req(c, req);
	return (NULL);
}

/* Setup tag contents and structure  */
uint16_t
p9_tag_create(struct p9_client *clnt)
{
	int tag;

	tag = alloc_unr(&clnt->tagpool);
	P9_DEBUG(LPROTO, "%s: clnt %p: tag %d\n", __func__, clnt, tag);

	/* Alloc_unr returning -1 is an error for no units left */
	if (tag == -1) {
		return (P9_NOTAG);
	}
	return (tag);
}

/* Clean up tag structures */
void
p9_tag_destroy(struct p9_client *clnt, uint16_t tag)
{

	P9_DEBUG(LPROTO, "%s: clnt %p: tag %d\n", __func__, clnt, tag);

	/* Release to the pool */
	free_unr(&clnt->tagpool, tag);
}

/* Allocate a new fid from the fidpool */
struct p9_fid *
p9_fid_create(struct p9_client *clnt)
{
	struct p9_fid *fid;


	fid = uma_zalloc(p9fs_fid_zone, M_WAITOK | M_ZERO);
	fid->fid = alloc_unr(&clnt->fidpool);
	P9_DEBUG(LPROTO, "%s: fid %d\n", __func__, fid->fid);

	/* Alloc_unr returning -1 is an error for no units left */
	if (fid->fid == -1) {
		uma_zfree(p9fs_fid_zone, fid);
		return (NULL);
	}
	fid->mode = -1;
	fid->uid = -1;
	fid->clnt = clnt;

	return (fid);
}

/* Free the fid by releasing it to fidpool */
void
p9_fid_destroy(struct p9_fid *fid)
{
	struct p9_client *clnt;

	P9_DEBUG(LPROTO, "%s: fid %d\n", __func__, fid->fid);
	clnt = fid->clnt;
	/* Release to the pool */
	free_unr(&clnt->fidpool, fid->fid);
	uma_zfree(p9fs_fid_zone, fid);
}

/* Request the version of 9P protocol */
int
p9_client_version(struct p9_client *c)
{
	int error;
	struct p9_req_t *req;
	char *version;
	int msize;

	error = 0;

	P9_DEBUG(PROTO, "TVERSION msize %d protocol %d\n",
	    c->msize, c->proto_version);

	switch (c->proto_version) {
	case p9_proto_2000L:
		req = p9_client_request(c, P9PROTO_TVERSION, &error, "ds",
		    c->msize, "9P2000.L");
		break;
	case p9_proto_2000u:
		req = p9_client_request(c, P9PROTO_TVERSION, &error, "ds",
		    c->msize, "9P2000.u");
		break;
	case p9_proto_legacy:
		req = p9_client_request(c, P9PROTO_TVERSION, &error, "ds",
		    c->msize, "9P2000");
		break;
	default:
		return (EINVAL);
	}

	/*  Always return the relevant error code */
	if (error != 0)
		return (error);

	error = p9_buf_readf(req->rc, c->proto_version, "ds", &msize, &version);
	if (error != 0) {
		P9_DEBUG(ERROR, "%s: version error: %d\n", __func__, error);
		goto out;
	}

	P9_DEBUG(PROTO, "RVERSION msize %d %s\n", msize, version);

	if (!strncmp(version, "9P2000.L", 8))
		c->proto_version = p9_proto_2000L;
	else if (!strncmp(version, "9P2000.u", 8))
		c->proto_version = p9_proto_2000u;
	else if (!strncmp(version, "9P2000", 6))
		c->proto_version = p9_proto_legacy;
	else {
		error = ENOMEM;
		goto out;
	}

	/* limit the msize .*/
	if (msize < c->msize)
		c->msize = msize;
out:
	p9_free_req(c, req);
	return (error);
}

/*
 * Initialize zones for different things. This is called from Init module
 * so that we just have them initalized once.
 */
void
p9_init_zones(void)
{

	/* Create the request and the fid zones */
	p9fs_fid_zone = uma_zcreate("p9fs fid zone",
	    sizeof(struct p9_fid), NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, 0);

	/* Create the request and the fid zones */
	p9fs_req_zone = uma_zcreate("p9fs req zone",
	    sizeof(struct p9_req_t), NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, 0);

	/* Create the buffer zone */
	p9fs_buf_zone = uma_zcreate("p9fs buf zone",
	    sizeof(struct p9_buffer) + P9FS_MTU, NULL, NULL,
	    NULL, NULL, UMA_ALIGN_PTR, 0);
}

void
p9_destroy_zones(void)
{

	uma_zdestroy(p9fs_fid_zone);
	uma_zdestroy(p9fs_req_zone);
	uma_zdestroy(p9fs_buf_zone);
}

/* Return the client to the session in the FS to hold it */
struct p9_client *
p9_client_create(struct mount *mp, int *error, const char *mount_tag)
{
	struct p9_client *clnt;

	clnt = malloc(sizeof(struct p9_client), M_P9CLNT, M_WAITOK | M_ZERO);
	mtx_init(&clnt->clnt_mtx, "p9clnt", NULL, MTX_DEF);

	/* Parse should have set trans_mod */
	*error = p9_parse_opts(mp, clnt);
	if (*error != 0)
		goto out;

	if (clnt->ops == NULL) {
		*error = EINVAL;
		P9_DEBUG(ERROR, "%s: no transport\n", __func__);
		goto out;
	}

	/* All the structures from here are protected by the lock clnt_mtx */
	init_unrhdr(&clnt->fidpool, P9FS_ROOT_FID_NO, P9FS_MAX_FID_CNT,
	    &clnt->clnt_mtx);
	init_unrhdr(&clnt->tagpool, P9FS_MIN_TAG, P9FS_MAX_TAG,
	    &clnt->clnt_mtx);

	P9_DEBUG(TRANS, "%s: clnt %p trans %p msize %d protocol %d\n",
	    __func__, clnt, clnt->ops, clnt->msize, clnt->proto_version);

	*error = clnt->ops->create(mount_tag, &clnt->handle);
	if (*error != 0) {
		P9_DEBUG(ERROR, "%s: transport create failed .%d \n",
		    __func__, *error);
		goto out;
	}
	clnt->trans_status = P9FS_CONNECT;

	*error = p9_client_version(clnt);
	if (*error != 0)
		goto out;

	P9_DEBUG(TRANS, "%s: client creation succeeded.\n", __func__);
	return (clnt);
out:
	free(clnt, M_P9CLNT);
	return (NULL);
}

/* Destroy the client by destroying associated fidpool and tagpool */
void
p9_client_destroy(struct p9_client *clnt)
{

	P9_DEBUG(TRANS, "%s: client %p\n", __func__, clnt);
	clnt->ops->close(clnt->handle);

	P9_DEBUG(TRANS, "%s : Destroying fidpool\n", __func__);
	clear_unrhdr(&clnt->fidpool);

	P9_DEBUG(TRANS, "%s : Destroying tagpool\n", __func__);
	clear_unrhdr(&clnt->tagpool);

	free(clnt, M_P9CLNT);
}

/*
 * Attach a user to the filesystem. Create a fid for that user to access
 * the root of the filesystem.
 */
struct p9_fid *
p9_client_attach(struct p9_client *clnt, struct p9_fid *afid,
    const char *uname, uid_t n_uname, const char *aname, int *error)
{
	struct p9_req_t *req;
	struct p9_fid *fid;
	struct p9_qid qid;

	P9_DEBUG(PROTO, "TATTACH uname=%s aname=%s, n_uname=%d\n",
	    uname, aname, n_uname);
	fid = p9_fid_create(clnt);
	if (fid == NULL) {
		*error = ENOMEM;
		return (NULL);
	}
	fid->uid = n_uname;

	req = p9_client_request(clnt, P9PROTO_TATTACH, error, "ddssd", fid->fid,
	    P9PROTO_NOFID, uname, aname, n_uname);
	if (*error != 0)
		goto out;

	*error = p9_buf_readf(req->rc, clnt->proto_version, "Q", &qid);
	if (*error != 0) {
		P9_DEBUG(ERROR, "%s: p9_buf_readf failed: %d \n",
		    __func__, *error);
		goto out;
	}

	P9_DEBUG(PROTO, "RATTACH qid %x.%llx.%x\n",
	    qid.type, (unsigned long long)qid.path, qid.version);

	memmove(&fid->qid, &qid, sizeof(struct p9_qid));
	p9_free_req(clnt, req);

	return (fid);
out:
	if (req != NULL)
		p9_free_req(clnt, req);
	if (fid != NULL)
		p9_fid_destroy(fid);

	return (NULL);
}

/* Delete a file/directory. Corresponding fid will be cluncked too */
int
p9_client_remove(struct p9_fid *fid)
{
	int error;
	struct p9_client *clnt;
	struct p9_req_t *req;

	P9_DEBUG(PROTO, "TREMOVE fid %d\n", fid->fid);

	error = 0;
	clnt = fid->clnt;

	req = p9_client_request(clnt, P9PROTO_TREMOVE, &error, "d", fid->fid);
	if (error != 0) {
		P9_DEBUG(PROTO, "RREMOVE fid %d\n", fid->fid);
		return (error);
	}

	p9_free_req(clnt, req);
	return (error);
}

int
p9_client_unlink(struct p9_fid *dfid, const char *name, int32_t flags)
{
	int error;
	struct p9_client *clnt;
	struct p9_req_t *req;

	error = 0;
	clnt = dfid->clnt;

	req = p9_client_request(clnt, P9PROTO_TUNLINKAT, &error, "dsd",
	    dfid->fid, name, flags);
	if (error != 0) {
		P9_DEBUG(PROTO, "RUNLINKAT fid %d\n", dfid->fid);
		return (error);
	}

	p9_free_req(clnt, req);
	return (error);
}

/* Inform the file server that the current file represented by fid is no longer
 * needed by the client. Any allocated fid on the server needs a clunk to be
 * destroyed.
 */
int
p9_client_clunk(struct p9_fid *fid)
{
	int error;
	struct p9_client *clnt;
	struct p9_req_t *req;

	error = 0;

	if (fid == NULL) {
		P9_DEBUG(ERROR, "%s: clunk with NULL fid is bad\n", __func__);
		return (0);
	}

	P9_DEBUG(PROTO, "TCLUNK fid %d \n", fid->fid);

	clnt = fid->clnt;
	req = p9_client_request(clnt, P9PROTO_TCLUNK, &error, "d", fid->fid);
	if (req != NULL) {
		P9_DEBUG(PROTO, "RCLUNK fid %d\n", fid->fid);
		p9_free_req(clnt, req);
	}

	p9_fid_destroy(fid);
	return (error);
}

/*
 * Client_walk is for searching any component name in a directory.
 * This is usually called on lookups. Also when we need a new open fid
 * as 9p needs to have an open fid for every file to fileops, we call this
 * validate the component of the file and return the newfid(openfid) created.
 */
struct p9_fid *
p9_client_walk(struct p9_fid *oldfid, uint16_t nwnames, char **wnames,
    int clone, int *error)
{
	struct p9_client *clnt;
	struct p9_fid *fid;
	struct p9_qid *wqids;
	struct p9_req_t *req;
	uint16_t nwqids, count;

	clnt = oldfid->clnt;
	wqids = NULL;
	nwqids = 0;

	/*
	 *  Before, we go and create fid, make sure we are not tearing
	 *  down. Only then we create.
	 *  Allow only cleanup clunk messages once we are starting to teardown.
	 */
	if (clnt->trans_status != P9FS_CONNECT) {
		*error = EIO;
		return (NULL);
	}

	if (clone) {
		fid = p9_fid_create(clnt);
		if (fid == NULL) {
			*error = ENOMEM;
			return (NULL);
		}
		fid->uid = oldfid->uid;
	} else
		fid = oldfid;

	P9_DEBUG(PROTO, "TWALK fids %d,%d nwnames %u wname %s\n",
	    oldfid->fid, fid->fid, nwnames,
	    wnames != NULL ? wnames[nwnames-1] : NULL);

	/*
	 * The newfid is for the component in search. We are preallocating as
	 * qemu on other side allocates or returns a fid if it sees a match
	 */
	req = p9_client_request(clnt, P9PROTO_TWALK, error, "ddT", oldfid->fid,
	    fid->fid, wnames, nwnames);
	if (*error != 0) {
		if (fid != oldfid)
			p9_fid_destroy(fid);
		return (NULL);
	}

	*error = p9_buf_readf(req->rc, clnt->proto_version, "R", &nwqids,
	    &wqids);
	if (*error != 0)
		goto out;

	P9_DEBUG(PROTO, "RWALK nwqid %d:\n", nwqids);

	if (nwqids != nwnames) {
		*error = ENOENT;
		goto out;
	}

	for (count = 0; count < nwqids; count++)
		P9_DEBUG(TRANS, "%s: [%d] %x.%llx.%x\n",
		    __func__, count, wqids[count].type,
		    (unsigned long long)wqids[count].path,
		    wqids[count].version);

	if (nwnames)
		memmove(&fid->qid, &wqids[nwqids - 1], sizeof(struct p9_qid));
	else
		fid->qid = oldfid->qid;

	p9_free_req(clnt, req);
	free(wqids, M_TEMP);
	return (fid);

out:
	p9_free_req(clnt, req);
	if (wqids)
		free(wqids, M_TEMP);
	if (fid && fid != oldfid)
		p9_client_clunk(fid);
	return (NULL);
}

/* Open a file with given fid and mode */
int
p9_client_open(struct p9_fid *fid, int mode)
{
	int error, mtu;
	struct p9_client *clnt;
	struct p9_req_t *req;

	error = 0;
	clnt = fid->clnt;
	mtu = 0;

	P9_DEBUG(PROTO, "%s fid %d mode %d\n",
	    p9_is_proto_dotl(clnt) ? "TLOPEN" : "TOPEN",
	    fid->fid, mode);

	if (fid->mode != -1)
		return (EINVAL);

	if (p9_is_proto_dotl(clnt))
		req = p9_client_request(clnt, P9PROTO_TLOPEN, &error, "dd",
		    fid->fid, mode);
	else
		req = p9_client_request(clnt, P9PROTO_TOPEN, &error, "db",
		    fid->fid, mode);

	if (error != 0)
		return (error);

	error = p9_buf_readf(req->rc, clnt->proto_version, "Qd", &fid->qid,
	    &mtu);
	if (error != 0)
		goto out;

	P9_DEBUG(PROTO, "%s qid %x.%llx.%x mtu %x\n",
	    p9_is_proto_dotl(clnt) ? "RLOPEN" : "ROPEN",
	    (fid->qid).type, (unsigned long long)(fid->qid).path,
	    (fid->qid).version, mtu);

	fid->mode = mode;
	fid->mtu = mtu;
out:
	p9_free_req(clnt, req);
	return (error);
}

/* Request to get directory entries */
int
p9_client_readdir(struct p9_fid *fid, char *data, uint64_t offset,
    uint32_t count)
{
	int error;
	uint32_t rsize;
	struct p9_client *clnt;
	struct p9_req_t *req;
	char *dataptr;

	P9_DEBUG(PROTO, "TREADDIR fid %d offset %llu count %d\n",
	    fid->fid, (unsigned long long) offset, count);

	error = 0;
	rsize = fid->mtu;
	clnt = fid->clnt;

	if (rsize == 0 || rsize > clnt->msize)
		rsize = clnt->msize;

	if (count < rsize)
		rsize = count;

	req = p9_client_request(clnt, P9PROTO_TREADDIR, &error, "dqd",
	    fid->fid, offset, rsize);

	if (error != 0) {
		P9_DEBUG(ERROR, "%s: couldn't allocate req in client_readdir\n",
			__func__);
		return (-error);
	}

	error = p9_buf_readf(req->rc, clnt->proto_version, "D", &count,
	    &dataptr);
	if (error != 0) {
		P9_DEBUG(ERROR, "%s: p0_buf_readf failed: %d\n",
		    __func__, error);
		p9_free_req(clnt, req);
		return (-error);
	}

	P9_DEBUG(PROTO, "RREADDIR count %u\n", count);

	/* Copy back the data into the input buffer. */
	memmove(data, dataptr, count);
	p9_free_req(clnt, req);
	return (count);
}

/*
 * Read count bytes from offset for the file fid into the character
 * buffer data. This buffer is handed over to p9fs to process into user
 * buffers. Note that this function typically returns the number of bytes read
 * so in case of an error we return -error so that we can distinguish between
 * error codes and bytes.
 */
int
p9_client_read(struct p9_fid *fid, uint64_t offset, uint32_t count, char *data)
{
	struct p9_client *clnt;
	struct p9_req_t *req;
	char *dataptr;
	int error, rsize;

	clnt = fid->clnt;
	rsize = fid->mtu;
	error = 0;

	P9_DEBUG(PROTO, "TREAD fid %d offset %llu %u\n",
	    fid->fid, (unsigned long long) offset, count);

	if (!rsize || rsize > clnt->msize)
		rsize = clnt->msize;

	if (count < rsize)
		rsize = count;

	/* At this stage, we only have 8K buffers so only transfer */
	req = p9_client_request(clnt, P9PROTO_TREAD, &error, "dqd", fid->fid,
	    offset, rsize);
	if (error != 0) {
		P9_DEBUG(ERROR, "%s: failed allocate request\n", __func__);
		return (-error);
	}

	error = p9_buf_readf(req->rc, clnt->proto_version, "D", &count,
	    &dataptr);
	if (error != 0) {
		P9_DEBUG(ERROR, "%s: p9_buf_readf failed: %d\n",
		    __func__, error);
		goto out;
	}

	if (rsize < count) {
		P9_DEBUG(PROTO, "RREAD count (%d > %d)\n", count, rsize);
		count = rsize;
	}

	P9_DEBUG(PROTO, "RREAD count %d\n", count);

	if (count == 0) {
		error = -EIO;
		P9_DEBUG(ERROR, "%s: EIO error in client_read \n", __func__);
		goto out;
	}

	/* Copy back the data into the input buffer. */
	memmove(data, dataptr, count);
	p9_free_req(clnt, req);
	return (count);
out:
	p9_free_req(clnt, req);
	return (-error);
}

/*
 * Write count bytes from buffer to the offset for the file fid
 * Note that this function typically returns the number of bytes written
 * so in case of an error we return -error so that we can distinguish between
 * error codes and bytes.
 */

int
p9_client_write(struct p9_fid *fid, uint64_t offset, uint32_t count, char *data)
{
	struct p9_client *clnt;
	struct p9_req_t *req;
	int ret, error, rsize;

	clnt = fid->clnt;
	rsize = fid->mtu;
	ret = 0;
	error = 0;

	P9_DEBUG(PROTO, "TWRITE fid %d offset %llu  %u\n",
	    fid->fid, (unsigned long long) offset, count);

	if (!rsize || rsize > clnt->msize)
		rsize = clnt->msize;

	/* Limit set by Qemu ,8168 */
	if (count > rsize) {
		count = rsize;
	}

	/*
	 * Doing the Data blob instead. If at all we add the zerocopy, we can
	 * change it to uio direct copy
	 */
	req = p9_client_request(clnt, P9PROTO_TWRITE, &error, "dqD", fid->fid,
	    offset, count, data);
	if (error != 0) {
		P9_DEBUG(ERROR, "%s: failed allocate request: %d\n",
		    __func__, error);
		return (-error);
	}

	error = p9_buf_readf(req->rc, clnt->proto_version, "d", &ret);
	if (error != 0) {
		P9_DEBUG(ERROR, "%s: p9_buf_readf error: %d\n",
		    __func__, error);
		goto out;
	}

	if (count < ret) {
		P9_DEBUG(PROTO, "RWRITE count (%d > %d)\n", count, ret);
		ret = count;
	}
	P9_DEBUG(PROTO, "RWRITE count %d\n", ret);

	if (count == 0) {
		error = EIO;
		P9_DEBUG(ERROR, "%s: EIO error\n", __func__);
		goto out;
	}

	p9_free_req(clnt, req);
	return (ret);
out:
	p9_free_req(clnt, req);
	return (-error);
}


/* Create file under directory fid, with name, permissions, mode. */
int
p9_client_file_create(struct p9_fid *fid, char *name, uint32_t perm, int mode,
    char *extension)
{
	int error;
	struct p9_client *clnt;
	struct p9_req_t *req;
	struct p9_qid qid;
	int mtu;

	P9_DEBUG(PROTO, "TCREATE fid %d name %s perm %d mode %d\n",
	    fid->fid, name, perm, mode);

	clnt = fid->clnt;
	error = 0;

	if (fid->mode != -1)
		return (EINVAL);

	req = p9_client_request(clnt, P9PROTO_TCREATE, &error, "dsdb?s",
	    fid->fid, name, perm, mode, extension);
	if (error != 0)
		return (error);

	error = p9_buf_readf(req->rc, clnt->proto_version, "Qd", &qid, &mtu);
	if (error != 0)
		goto out;

	P9_DEBUG(PROTO, "RCREATE qid %x.%jx.%x mtu %x\n",
	    qid.type, (uintmax_t)qid.path, qid.version, mtu);
	fid->mode = mode;
	fid->mtu = mtu;

out:
	p9_free_req(clnt, req);
	return (error);
}

/* Request file system information of the file system */
int
p9_client_statfs(struct p9_fid *fid, struct p9_statfs *stat)
{
	int error;
	struct p9_req_t *req;
	struct p9_client *clnt;

	error = 0;
	clnt = fid->clnt;

	P9_DEBUG(PROTO, "TSTATFS fid %d\n", fid->fid);

	req = p9_client_request(clnt, P9PROTO_TSTATFS, &error, "d", fid->fid);
	if (error != 0) {
		return (error);
	}

	error = p9_buf_readf(req->rc, clnt->proto_version, "ddqqqqqqd",
	    &stat->type, &stat->bsize, &stat->blocks, &stat->bfree,
	    &stat->bavail, &stat->files, &stat->ffree, &stat->fsid,
	    &stat->namelen);

	if (error != 0)
		goto out;

	P9_DEBUG(PROTO, "RSTATFS fid %d type 0x%jx bsize %ju "
	    "blocks %ju bfree %ju bavail %ju files %ju ffree %ju "
	    "fsid %ju namelen %ju\n",
	    fid->fid, (uintmax_t)stat->type,
	    (uintmax_t)stat->bsize, (uintmax_t)stat->blocks,
	    (uintmax_t)stat->bfree, (uintmax_t)stat->bavail,
	    (uintmax_t)stat->files, (uintmax_t)stat->ffree,
	    (uintmax_t)stat->fsid, (uintmax_t)stat->namelen);

out:
	p9_free_req(clnt, req);
	return (error);
}

/* Rename file referenced by the fid */
int
p9_client_renameat(struct p9_fid *oldfid, char *oldname, struct p9_fid *newfid,
    char *newname)
{
	int error;
	struct p9_client *clnt;
	struct p9_req_t *req;

	P9_DEBUG(PROTO, "TRENAMEAT oldfid %d oldname %s newfid %d newfid %s",
	    oldfid->fid, oldname, newfid->fid, newname);

	error = 0;
	clnt = oldfid->clnt;

	/*
	 * we are calling the request with TRENAMEAT tag and not TRENAME with
	 * the 9p protocol version 9p2000.u as the QEMU version supports this
	 * version of renaming
	 */
	req = p9_client_request(clnt, P9PROTO_TRENAMEAT, &error, "dsds",
	    oldfid->fid, oldname, newfid->fid, newname);

	if (error != 0)
		return (error);

	p9_free_req(clnt, req);
	return (error);
}

/* Request to create symbolic link */
int
p9_create_symlink(struct p9_fid *fid, char *name, char *symtgt, gid_t gid)
{
	int error;
	struct p9_req_t *req;
	struct p9_client *clnt;
	struct p9_qid qid;

	error = 0;
	clnt = fid->clnt;

	P9_DEBUG(PROTO, "TSYMLINK fid %d name %s\n", fid->fid, name);

	req = p9_client_request(clnt, P9PROTO_TSYMLINK, &error, "dssd",
	    fid->fid, name, symtgt, gid);

	if (error != 0)
		return (error);

	error = p9_buf_readf(req->rc, clnt->proto_version, "Q", &qid);
	if (error != 0) {
		P9_DEBUG(ERROR, "%s: buf_readf failed %d\n", __func__, error);
		return (error);
	}

	P9_DEBUG(PROTO, "RSYMLINK qid %x.%jx.%x\n",
	    qid.type, (uintmax_t)qid.path, qid.version);

	p9_free_req(clnt, req);
	return (0);
}

/* Request to create hard link */
int
p9_create_hardlink(struct p9_fid *dfid, struct p9_fid *oldfid, char *name)
{
	int error;
	struct p9_req_t *req;
	struct p9_client *clnt;

	error = 0;
	clnt = dfid->clnt;

	P9_DEBUG(PROTO, "TLINK dfid %d oldfid %d name %s\n",
	    dfid->fid, oldfid->fid, name);

	req = p9_client_request(clnt, P9PROTO_TLINK, &error, "dds", dfid->fid,
	    oldfid->fid, name);
	if (error != 0)
		return (error);

	p9_free_req(clnt, req);
	return (0);
}

/* Request to read contents of symbolic link */
int
p9_readlink(struct p9_fid *fid, char **target)
{
	int error;
	struct p9_client *clnt;
	struct p9_req_t *req;

	error = 0;
	clnt = fid->clnt;

	P9_DEBUG(PROTO, "TREADLINK fid %d\n", fid->fid);

	req = p9_client_request(clnt, P9PROTO_TREADLINK, &error, "d", fid->fid);
	if (error != 0)
		return (error);

	error = p9_buf_readf(req->rc, clnt->proto_version, "s", target);
	if (error != 0) {
		P9_DEBUG(ERROR, "%s: buf_readf failed %d\n", __func__, error);
		return (error);
	}

	P9_DEBUG(PROTO, "RREADLINK target %s \n", *target);

	p9_free_req(clnt, req);
	return (0);
}

/* Get file attributes of the file referenced by the fid */
int
p9_client_getattr(struct p9_fid *fid, struct p9_stat_dotl *stat_dotl,
    uint64_t request_mask)
{
	int err;
	struct p9_client *clnt;
	struct p9_req_t *req;

	err = 0;

	P9_DEBUG(PROTO, "TGETATTR fid %d mask %ju\n",
	    fid->fid, (uintmax_t)request_mask);

	clnt = fid->clnt;
	req = p9_client_request(clnt, P9PROTO_TGETATTR, &err, "dq", fid->fid,
	    request_mask);
	if (req == NULL) {
		P9_DEBUG(ERROR, "%s: allocation failed %d", __func__, err);
		goto error;
	}

	err = p9_buf_readf(req->rc, clnt->proto_version, "A", stat_dotl);
	if (err != 0) {
		P9_DEBUG(ERROR, "%s: buf_readf failed %d\n", __func__, err);
		goto error;
	}

	p9_free_req(clnt, req);
	P9_DEBUG(PROTO, "RGETATTR fid %d qid %x.%jx.%x st_mode %8.8x "
	    "uid %d gid %d nlink %ju rdev %jx st_size %jx blksize %ju "
	    "blocks %ju st_atime_sec %ju, st_atime_nsec %ju "
	    "st_mtime_sec %ju, st_mtime_nsec %ju st_ctime_sec %ju "
	    "st_ctime_nsec %ju st_btime_sec %ju, st_btime_nsec %ju "
	    "st_stat %ju, st_data_version %ju \n", fid->fid,
	    stat_dotl->qid.type, (uintmax_t)stat_dotl->qid.path,
	    stat_dotl->qid.version, stat_dotl->st_mode, stat_dotl->st_uid,
	    stat_dotl->st_gid, (uintmax_t)stat_dotl->st_nlink,
	    (uintmax_t)stat_dotl->st_rdev, (uintmax_t)stat_dotl->st_size,
	    (uintmax_t)stat_dotl->st_blksize,
	    (uintmax_t)stat_dotl->st_blocks, (uintmax_t)stat_dotl->st_atime_sec,
	    (uintmax_t)stat_dotl->st_atime_nsec, (uintmax_t)stat_dotl->st_mtime_sec,
	    (uintmax_t)stat_dotl->st_mtime_nsec, (uintmax_t)stat_dotl->st_ctime_sec,
	    (uintmax_t)stat_dotl->st_ctime_nsec, (uintmax_t)stat_dotl->st_btime_sec,
	    (uintmax_t)stat_dotl->st_btime_nsec, (uintmax_t)stat_dotl->st_gen,
	    (uintmax_t)stat_dotl->st_data_version);

	return (err);

error:
	if (req != NULL)
		p9_free_req(clnt, req);

	return (err);
}

/* Set file attributes of the file referenced by the fid */
int
p9_client_setattr(struct p9_fid *fid, struct p9_iattr_dotl *p9attr)
{
	int err;
	struct p9_req_t *req;
	struct p9_client *clnt;

	err = 0;

	P9_DEBUG(PROTO, "TSETATTR fid %d"
	    " valid %x mode %x uid %d gid %d size %ju"
	    " atime_sec %ju atime_nsec %ju"
	    " mtime_sec %ju mtime_nsec %ju\n",
	    fid->fid,
	    p9attr->valid, p9attr->mode, p9attr->uid, p9attr->gid,
	    (uintmax_t)p9attr->size, (uintmax_t)p9attr->atime_sec,
	    (uintmax_t)p9attr->atime_nsec, (uintmax_t)p9attr->mtime_sec,
	    (uintmax_t)p9attr->mtime_nsec);

	clnt = fid->clnt;

	/* Any client_request error is converted to req == NULL error*/
	req = p9_client_request(clnt, P9PROTO_TSETATTR, &err, "dA", fid->fid,
	    p9attr);

	if (req == NULL) {
		P9_DEBUG(ERROR, "%s: allocation failed %d\n", __func__, err);
		goto error;
	}

	p9_free_req(clnt, req);
error:
	return (err);
}

