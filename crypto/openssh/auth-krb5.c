/*
 *    Kerberos v5 authentication and ticket-passing routines.
 */

#include "includes.h"
RCSID("$OpenBSD: auth-krb5.c,v 1.6 2002/03/04 17:27:39 stevesk Exp $");
RCSID("$FreeBSD$");

#include "ssh.h"
#include "ssh1.h"
#include "packet.h"
#include "xmalloc.h"
#include "log.h"
#include "servconf.h"
#include "uidswap.h"
#include "auth.h"

#ifdef KRB5
#include <krb5.h>

extern ServerOptions	 options;

static int
krb5_init(void *context)
{
	Authctxt *authctxt = (Authctxt *)context;
	krb5_error_code problem;
	static int cleanup_registered = 0;

	if (authctxt->krb5_ctx == NULL) {
		problem = krb5_init_context(&authctxt->krb5_ctx);
		if (problem)
			return (problem);
		krb5_init_ets(authctxt->krb5_ctx);
	}
	if (!cleanup_registered) {
		fatal_add_cleanup(krb5_cleanup_proc, authctxt);
		cleanup_registered = 1;
	}
	return (0);
}

/*
 * Try krb5 authentication. server_user is passed for logging purposes
 * only, in auth is received ticket, in client is returned principal
 * from the ticket
 */
int
auth_krb5(Authctxt *authctxt, krb5_data *auth, char **client)
{
	krb5_error_code problem;
	krb5_principal server;
	krb5_data reply;
	krb5_ticket *ticket;
	int fd, ret;

	ret = 0;
	server = NULL;
	ticket = NULL;
	reply.length = 0;

	problem = krb5_init(authctxt);
	if (problem)
		goto err;

	problem = krb5_auth_con_init(authctxt->krb5_ctx,
	    &authctxt->krb5_auth_ctx);
	if (problem)
		goto err;

	fd = packet_get_connection_in();
	problem = krb5_auth_con_setaddrs_from_fd(authctxt->krb5_ctx,
	    authctxt->krb5_auth_ctx, &fd);
	if (problem)
		goto err;

	problem = krb5_sname_to_principal(authctxt->krb5_ctx,  NULL, NULL ,
	    KRB5_NT_SRV_HST, &server);
	if (problem)
		goto err;

	problem = krb5_rd_req(authctxt->krb5_ctx, &authctxt->krb5_auth_ctx,
	    auth, server, NULL, NULL, &ticket);
	if (problem)
		goto err;

	problem = krb5_copy_principal(authctxt->krb5_ctx, ticket->client,
	    &authctxt->krb5_user);
	if (problem)
		goto err;

	/* if client wants mutual auth */
	problem = krb5_mk_rep(authctxt->krb5_ctx, authctxt->krb5_auth_ctx,
	    &reply);
	if (problem)
		goto err;

	/* Check .k5login authorization now. */
	if (!krb5_kuserok(authctxt->krb5_ctx, authctxt->krb5_user,
	    authctxt->pw->pw_name))
		goto err;

	if (client)
		krb5_unparse_name(authctxt->krb5_ctx, authctxt->krb5_user,
		    client);

	packet_start(SSH_SMSG_AUTH_KERBEROS_RESPONSE);
	packet_put_string((char *) reply.data, reply.length);
	packet_send();
	packet_write_wait();

	ret = 1;
 err:
	if (server)
		krb5_free_principal(authctxt->krb5_ctx, server);
	if (ticket)
		krb5_free_ticket(authctxt->krb5_ctx, ticket);
	if (reply.length)
		xfree(reply.data);

	if (problem) {
		if (authctxt->krb5_ctx != NULL)
			debug("Kerberos v5 authentication failed: %s",
			    krb5_get_err_text(authctxt->krb5_ctx, problem));
		else
			debug("Kerberos v5 authentication failed: %d",
			    problem);
	}

	return (ret);
}

int
auth_krb5_tgt(Authctxt *authctxt, krb5_data *tgt)
{
	krb5_error_code problem;
	krb5_ccache ccache = NULL;
	char *pname;

	if (authctxt->pw == NULL || authctxt->krb5_user == NULL)
		return (0);

	temporarily_use_uid(authctxt->pw);

	problem = krb5_cc_gen_new(authctxt->krb5_ctx, &krb5_fcc_ops, &ccache);
	if (problem)
		goto fail;

	problem = krb5_cc_initialize(authctxt->krb5_ctx, ccache,
	    authctxt->krb5_user);
	if (problem)
		goto fail;

	problem = krb5_rd_cred2(authctxt->krb5_ctx, authctxt->krb5_auth_ctx,
	    ccache, tgt);
	if (problem)
		goto fail;

	authctxt->krb5_fwd_ccache = ccache;
	ccache = NULL;

	authctxt->krb5_ticket_file = (char *)krb5_cc_get_name(authctxt->krb5_ctx, authctxt->krb5_fwd_ccache);

	problem = krb5_unparse_name(authctxt->krb5_ctx, authctxt->krb5_user,
	    &pname);
	if (problem)
		goto fail;

	debug("Kerberos v5 TGT accepted (%s)", pname);

	restore_uid();

	return (1);

 fail:
	if (problem)
		debug("Kerberos v5 TGT passing failed: %s",
		    krb5_get_err_text(authctxt->krb5_ctx, problem));
	if (ccache)
		krb5_cc_destroy(authctxt->krb5_ctx, ccache);

	restore_uid();

	return (0);
}

int
auth_krb5_password(Authctxt *authctxt, const char *password)
{
	krb5_error_code problem;

	if (authctxt->pw == NULL)
		return (0);

	temporarily_use_uid(authctxt->pw);

	problem = krb5_init(authctxt);
	if (problem)
		goto out;

	problem = krb5_parse_name(authctxt->krb5_ctx, authctxt->pw->pw_name,
		    &authctxt->krb5_user);
	if (problem)
		goto out;

	problem = krb5_cc_gen_new(authctxt->krb5_ctx, &krb5_mcc_ops,
	    &authctxt->krb5_fwd_ccache);
	if (problem)
		goto out;

	problem = krb5_cc_initialize(authctxt->krb5_ctx,
	    authctxt->krb5_fwd_ccache, authctxt->krb5_user);
	if (problem)
		goto out;

	restore_uid();
	problem = krb5_verify_user(authctxt->krb5_ctx, authctxt->krb5_user,
	    authctxt->krb5_fwd_ccache, password, 1, NULL);
	temporarily_use_uid(authctxt->pw);

	if (problem)
		goto out;

	authctxt->krb5_ticket_file = (char *)krb5_cc_get_name(authctxt->krb5_ctx, authctxt->krb5_fwd_ccache);

 out:
	restore_uid();

	if (problem) {
		if (authctxt->krb5_ctx != NULL)
			debug("Kerberos password authentication failed: %s",
			    krb5_get_err_text(authctxt->krb5_ctx, problem));
		else
			debug("Kerberos password authentication failed: %d",
			    problem);

		krb5_cleanup_proc(authctxt);

		if (options.kerberos_or_local_passwd)
			return (-1);
		else
			return (0);
	}
	return (1);
}

void
krb5_cleanup_proc(void *context)
{
	Authctxt *authctxt = (Authctxt *)context;

	debug("krb5_cleanup_proc called");
	if (authctxt->krb5_fwd_ccache) {
		krb5_cc_destroy(authctxt->krb5_ctx, authctxt->krb5_fwd_ccache);
		authctxt->krb5_fwd_ccache = NULL;
	}
	if (authctxt->krb5_user) {
		krb5_free_principal(authctxt->krb5_ctx, authctxt->krb5_user);
		authctxt->krb5_user = NULL;
	}
	if (authctxt->krb5_auth_ctx) {
		krb5_auth_con_free(authctxt->krb5_ctx,
		    authctxt->krb5_auth_ctx);
		authctxt->krb5_auth_ctx = NULL;
	}
	if (authctxt->krb5_ctx) {
		krb5_free_context(authctxt->krb5_ctx);
		authctxt->krb5_ctx = NULL;
	}
}

#endif /* KRB5 */
