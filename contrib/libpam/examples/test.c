/*
 * Marc Ewing (marc@redhat.com) - original test code 
 * Alexander O. Yuriev (alex@bach.cis.temple.edu)
 * Andrew Morgan (morgan@physics.ucla.edu)
 */

#include <stdlib.h>
#include <stdio.h>
#include <pwd.h>

#include <security/pam_appl.h>

/* this program is not written to the PAM spec: it tests the
 * pam_[sg]et_data() functions. Which is usually reserved for modules */

#include <security/pam_modules.h>
#include <security/pam_misc.h>

#define USERNAMESIZE	1024

static int test_conv(	int num_msg, 
			const struct pam_message **msgm,
			struct pam_response **response, 
			void *appdata_ptr	)
{
    return 0;
}

static struct pam_conv conv = {
    test_conv,
    NULL
};

static int cleanup_func(pam_handle_t *pamh, void *data, int error_status)
{
    printf("Cleaning up!\n");
    return PAM_SUCCESS;
}

void main( void )
{
	pam_handle_t *pamh;
	char	*name = ( char *) malloc( USERNAMESIZE + 1 );
	char	*p = NULL; 
	char	*s = NULL;

	if (! name )
		{
			perror( "Ouch, don't have enough memory");
			exit( -1 );
		}
    



	fprintf( stdout, "Enter a name of a user to authenticate : ");
	name = fgets( name , USERNAMESIZE, stdin );
	if ( !name )
		{
			perror ( "Hey, how can authenticate "
				 "someone whos name I don't know?" );
			exit ( -1 );
		}
	
	*( name + strlen ( name ) - 1 ) = 0;

	pam_start( "login", name, &conv, &pamh	);

	p = x_strdup( getpass ("Password: ") );
	if ( !p )
		{
			perror ( "You love NULL pointers, "
				 "don't you? I don't ");
			exit ( -1 );
		}
	pam_set_item ( pamh, PAM_AUTHTOK, p );
	pam_get_item ( pamh, PAM_USER, (void**) &s);
	pam_set_data(pamh, "DATA", "Hi there! I'm data!", cleanup_func);
	pam_get_data(pamh, "DATA", (void **) &s);
 	printf("%s\n", s);

	fprintf( stdout, "*** Attempting to perform "
		 "PAM authentication...\n");
	fprintf( stdout, "%s\n",
		 pam_strerror( pam_authenticate( pamh, 0 ) ) ) ;
    
	pam_end(pamh, PAM_SUCCESS);
}
