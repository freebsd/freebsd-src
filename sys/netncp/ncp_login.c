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
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/socket.h>

#include <netncp/ncp.h>
#include <netncp/ncp_conn.h>
#include <netncp/ncp_subr.h>
#include <netncp/ncp_ncp.h>
#include <netncp/ncp_rq.h>
#include <netncp/ncp_nls.h>
#include <netncp/nwerror.h>

static int  ncp_login_encrypted(struct ncp_conn *conn, struct ncp_bindery_object *object,
		    unsigned char *key, unsigned char *passwd,
		    struct proc *p, struct ucred *cred);
static int ncp_login_unencrypted(struct ncp_conn *conn, u_int16_t object_type, 
		    char *object_name, unsigned char *passwd,
		    struct proc *p, struct ucred *cred);
static int  ncp_sign_start(struct ncp_conn *conn, char *logindata);
static int  ncp_get_encryption_key(struct ncp_conn *conn, char *target);

/*
 * Initialize packet signatures. They a slightly modified MD4.
 * The first 16 bytes of logindata are the shuffled password,
 * the last 8 bytes the encryption key as received from the server.
 */
int
ncp_sign_start(struct ncp_conn *conn, char *logindata) {
	char msg[64];
	u_int32_t state[4];

	memcpy(msg, logindata, 24);
	memcpy(msg + 24, "Authorized NetWare Client", 25);
	bzero(msg + 24 + 25, sizeof(msg) - 24 - 25);

	conn->sign_state[0] = 0x67452301;
	conn->sign_state[1] = 0xefcdab89;
	conn->sign_state[2] = 0x98badcfe;
	conn->sign_state[3] = 0x10325476;
	ncp_sign(conn->sign_state, msg, state);
	conn->sign_root[0] = state[0];
	conn->sign_root[1] = state[1];
	conn->flags |= NCPFL_SIGNACTIVE;
	return 0;
}

/*
 * target is a 8-byte buffer
 */
int
ncp_get_encryption_key(struct ncp_conn *conn, char *target) {
	int error;
	DECLARE_RQ;

	NCP_RQ_HEAD_S(23,23,conn->procp,conn->ucred);
	checkbad(ncp_request(conn,rqp));
	if (rqp->rpsize < 8) {
		NCPFATAL("rpsize=%d < 8\n", rqp->rpsize);
		return EIO;
	}
	ncp_rp_mem(rqp, target, 8);
	NCP_RQ_EXIT;
	return error;
}

int
ncp_login_object(struct ncp_conn *conn, unsigned char *username, 
		int login_type, unsigned char *password,
		struct proc *p,struct ucred *cred)
{
	int error;
	unsigned char ncp_key[8];
	struct ncp_bindery_object user;

	if ((error = ncp_get_encryption_key(conn, ncp_key)) != 0) {
		printf("%s: Warning: use unencrypted login\n", __FUNCTION__);
		return ncp_login_unencrypted(conn, login_type, username, password,p,cred);
	}
	if ((error = ncp_get_bindery_object_id(conn, login_type, username, &user,p,cred)) != 0) {
		return error;
	}
	error = ncp_login_encrypted(conn, &user, ncp_key, password,p,cred);
	return error;
}

int
ncp_login_encrypted(struct ncp_conn *conn, struct ncp_bindery_object *object,
		    unsigned char *key, unsigned char *passwd,
		    struct proc *p,struct ucred *cred) {
	u_int32_t tmpID = htonl(object->object_id);
	u_char buf[16 + 8];
	u_char encrypted[8];
	int error;
	DECLARE_RQ;

	nw_keyhash((u_char*)&tmpID, passwd, strlen(passwd), buf);
	nw_encrypt(key, buf, encrypted);

	NCP_RQ_HEAD_S(23,24,p,cred);
	ncp_rq_mem(rqp, encrypted, 8);
	ncp_rq_word_hl(rqp, object->object_type);
	ncp_rq_pstring(rqp, object->object_name);
	error = ncp_request(conn, rqp);
	NCP_RQ_EXIT_NB;
	if (conn->flags & NCPFL_SIGNWANTED) {
		if (error == 0 || error == NWE_PASSWORD_EXPIRED) {
			memcpy(buf + 16, key, 8);
			error = ncp_sign_start(conn, buf);
		}
	}
	return error;
}

int
ncp_login_unencrypted(struct ncp_conn *conn, u_int16_t object_type, 
		    char *object_name, unsigned char *passwd,
		    struct proc *p, struct ucred *cred)
{
	int error;
	DECLARE_RQ;

	NCP_RQ_HEAD_S(23,20,conn->procp,conn->ucred);
	ncp_rq_word_hl(rqp, object_type);
	ncp_rq_pstring(rqp, object_name);
	ncp_rq_pstring(rqp, passwd);
	error = ncp_request(conn,rqp);
	NCP_RQ_EXIT_NB;
	return error;
}

/*
 * Login to specified server with username and password.
 * conn should be locked.
 */
int
ncp_login(struct ncp_conn *conn, char *user, int objtype, char *password,
	  struct proc *p, struct ucred *cred) {
	int error;

	if (ncp_suser(cred) != 0 && cred->cr_uid != conn->nc_owner->cr_uid)
		return EACCES;
	if (conn->flags & NCPFL_LOGGED) return EALREADY;
	if ((conn->flags & NCPFL_ATTACHED) == 0) return ENOTCONN;
	conn->li.user = ncp_str_dup(user);
	conn->li.password = ncp_str_dup(password);
	if (conn->li.user == NULL || conn->li.password == NULL) {
		error = EINVAL;
		goto bad;
	}
	ncp_str_upper(conn->li.user);
	if ((conn->li.opt & NCP_OPT_NOUPCASEPASS) == 0)
		ncp_str_upper(conn->li.password);
	checkbad(ncp_login_object(conn, conn->li.user, objtype, conn->li.password,p,cred));
	conn->li.objtype = objtype;
	conn->flags |= NCPFL_LOGGED;
	return 0;
bad:
	if (conn->li.user) free(conn->li.user, M_NCPDATA);
	if (conn->li.password) free(conn->li.password, M_NCPDATA);
	conn->li.user = conn->li.password = NULL;
	return error;
}
