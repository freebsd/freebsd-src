/*
  $Id: check_user.c,v 1.1 1996/11/10 21:19:30 morgan Exp morgan $
  
  This program was contributed by Shane Watts <shane@icarus.bofh.asn.au>
  slight modifications by AGM.

  You need to add the following (or equivalent) to the /etc/pam.conf file.
  # check authorization
  check   auth       required     pam_unix_auth.so
  check   account    required     pam_unix_acct.so

  $Log: check_user.c,v $
  Revision 1.1  1996/11/10 21:19:30  morgan
  Initial revision

 */

#include <security/pam_appl.h>
#include <security/pam_misc.h>
#include <stdio.h>

static struct pam_conv conv = {
    misc_conv,
    NULL
};

int main(int argc, char *argv[])
{
    pam_handle_t *pamh=NULL;
    int retval;
    const char *user="nobody";

    if(argc == 2) {
	user = argv[1];
    }

    if(argc > 2) {
	fprintf(stderr, "Usage: check_user [username]\n");
	exit(1);
    }

    retval = pam_start("check", user, &conv, &pamh);
	
    if (retval == PAM_SUCCESS)
        retval = pam_authenticate(pamh, 0);    /* is user really user? */

    if (retval == PAM_SUCCESS)
        retval = pam_acct_mgmt(pamh, 0);       /* permitted access? */

    /* This is where we have been authorized or not. */

    if (retval == PAM_SUCCESS) {
	fprintf(stdout, "Authenticated\n");
    } else {
	fprintf(stdout, "Not Authenticated\n");
    }

    if (pam_end(pamh,retval) != PAM_SUCCESS) {     /* close Linux-PAM */
	pamh = NULL;
	fprintf(stderr, "check_user: failed to release authenticator\n");
	exit(1);
    }

    return ( retval == PAM_SUCCESS ? 0:1 );       /* indicate success */
}
