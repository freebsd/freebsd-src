/*
 *    Kerberos v5 authentication and ticket-passing routines.
 * 
 * $FreeBSD$
 */

#include "includes.h"
#include "ssh.h"
#include "packet.h"
#include "xmalloc.h"

#ifdef KRB5

krb5_context ssh_context = NULL;
krb5_auth_context auth_context;
krb5_ccache mem_ccache = NULL; /* Credential cache for acquired ticket */

/* Try krb5 authentication. server_user is passed for logging purposes only,
   in auth is received ticket, in client is returned principal from the
   ticket */
int 
auth_krb5(const char* server_user, krb5_data *auth, krb5_principal *client)
{
	krb5_error_code problem;
	krb5_principal server = NULL;
	krb5_principal tkt_client = NULL;
	krb5_data reply;
	krb5_ticket *ticket = NULL;
	int fd;
	int ret;
	
	reply.length = 0;
	
	problem = krb5_init();
	if (problem) 
	   return 0;
	
	problem = krb5_auth_con_init(ssh_context, &auth_context);
	if (problem) {
	  log("Kerberos v5 authentication failed: %.100s",
	       krb5_get_err_text(ssh_context, problem));

	  return 0;
	}
	
       fd = packet_get_connection_in();
       problem = krb5_auth_con_setaddrs_from_fd(ssh_context, auth_context, &fd);
       if (problem) {
	 ret = 0;
	 goto err; 
       }
	
	problem = krb5_sname_to_principal(ssh_context,  NULL, NULL ,
	    KRB5_NT_SRV_HST, &server);
	if (problem) {
	    ret = 0;
	    goto err;
	}
	
	problem = krb5_rd_req(ssh_context, &auth_context, auth, server, NULL,
	    NULL, &ticket);
	if (problem) {
	  ret = 0;
	  goto err;
	}
	
	problem = krb5_copy_principal(ssh_context, ticket->client, &tkt_client);
	if (problem) {
	  ret = 0;
	  goto err;
	}
	
	/* if client wants mutual auth */
	problem = krb5_mk_rep(ssh_context, auth_context, &reply);
	if (problem) {
	  ret = 0;
	  goto err;
	}
	
	*client = tkt_client;
	
	packet_start(SSH_SMSG_AUTH_KERBEROS_RESPONSE);
	packet_put_string((char *) reply.data, reply.length);
	packet_send();
	packet_write_wait();
	ret = 1;
	  
err:
	if (server)
	  krb5_free_principal(ssh_context, server);
	if (ticket)
	  krb5_free_ticket(ssh_context, ticket);
	if (reply.length)
	  xfree(reply.data);
	return ret;
}

int
auth_krb5_tgt(char *server_user, krb5_data *tgt, krb5_principal tkt_client)
{
  krb5_error_code problem;
  krb5_ccache ccache = NULL;
  
  if (ssh_context == NULL) {
     goto fail;
  }
  
  problem = krb5_cc_gen_new(ssh_context, &krb5_mcc_ops, &ccache);
  if (problem) {
     goto fail;
  }
  
  problem = krb5_cc_initialize(ssh_context, ccache, tkt_client);
  if (problem) {
     goto fail;
  }
    
  problem = krb5_rd_cred2(ssh_context, auth_context, ccache, tgt);
  if (problem) {
     goto fail;
  }
  
  mem_ccache = ccache;
  ccache = NULL;
  
  /*
  problem = krb5_cc_copy_cache(ssh_context, ccache, mem_ccache);
  if (problem) {
     mem_ccache = NULL;
     goto fail; 
  }
  
  
  problem = krb5_cc_destroy(ssh_context, ccache);
  if (problem)
     goto fail;
     */
  
#if 0
  packet_start(SSH_SMSG_SUCCESS);
  packet_send();
  packet_write_wait();
#endif 
  return 1;
  
fail:
  if (ccache)
     krb5_cc_destroy(ssh_context, ccache);
#if 0
  packet_start(SSH_SMSG_FAILURE);
  packet_send();
  packet_write_wait();
#endif
  return 0;
}

int
auth_krb5_password(struct passwd *pw, const char *password)
{
  krb5_error_code problem;
  krb5_ccache ccache = NULL;
  krb5_principal client = NULL; 
  int ret;
  
  problem = krb5_init();
  if (problem)
     return 0;
  
  problem = krb5_parse_name(ssh_context, pw->pw_name, &client);
  if (problem) { 
     ret = 0;
     goto out;
  }

  problem = krb5_cc_gen_new(ssh_context, &krb5_mcc_ops, &ccache);
  if (problem) { 
     ret = 0;
     goto out;
  }
 
  problem = krb5_cc_initialize(ssh_context, ccache, client);
  if (problem) { 
     ret = 0;
     goto out;
  }
  
  problem = krb5_verify_user(ssh_context, client, ccache, password, 1, NULL);
  if (problem) { 
     ret = 0;
     goto out;
  }
  
/*
  problem = krb5_cc_copy_cache(ssh_context, ccache, mem_ccache);
  if (problem) { 
     ret = 0;
     mem_ccache = NULL;
     goto out;
  }
  */
  mem_ccache = ccache;
  ccache = NULL;
  
  ret = 1;
out:
  if (client != NULL)
     krb5_free_principal(ssh_context, client);
  if (ccache != NULL)
     krb5_cc_destroy(ssh_context, ccache);
  return ret;
}

void
krb5_cleanup_proc(void *ignore)
{
   extern krb5_principal tkt_client;
   
   debug("krb5_cleanup_proc() called");
   if (mem_ccache)
      krb5_cc_destroy(ssh_context, mem_ccache);
   if (tkt_client)
      krb5_free_principal(ssh_context, tkt_client);
   if (auth_context)
      krb5_auth_con_free(ssh_context, auth_context);
   if (ssh_context)
      krb5_free_context(ssh_context);
}
 
int
krb5_init(void)
{  
   krb5_error_code problem;
   static cleanup_registered = 0;
   
   if (ssh_context == NULL) {
      problem = krb5_init_context(&ssh_context);
      if (problem)
	 return problem;
      krb5_init_ets(ssh_context);
   }
  
   if (!cleanup_registered) {
      fatal_add_cleanup(krb5_cleanup_proc, NULL);
     cleanup_registered = 1;
   }
   return 0;
}
   
#endif /* KRB5 */
