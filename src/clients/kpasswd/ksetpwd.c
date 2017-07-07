/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#include <k5-platform.h>
#include <krb5.h>
#include <unistd.h>
#include <time.h>

#define TKTTIMELEFT     60*10   /* ten minutes */

static int verify_creds()
{
    krb5_context    kcontext;
    krb5_ccache             ccache;
    krb5_error_code kres;

    kres = krb5_init_context(&kcontext);
    if( kres == 0 )
    {
        kres = krb5_cc_default( kcontext, &ccache );
        if( kres == 0 )
        {
            krb5_principal  user_princ;

            kres = krb5_cc_get_principal( kcontext, ccache, &user_princ );
            if( kres == 0 )
                krb5_free_principal( kcontext, user_princ );
            krb5_cc_close( kcontext, ccache );
        }
        krb5_free_context(kcontext);
    }
    return kres;
}

static void get_init_creds_opt_init( krb5_get_init_creds_opt *outOptions )
{
    krb5_preauthtype    preauth[] = { KRB5_PADATA_ENC_TIMESTAMP };
    krb5_enctype        etypes[] = {ENCTYPE_DES_CBC_MD5, ENCTYPE_DES_CBC_CRC};
    krb5_get_init_creds_opt_set_address_list(outOptions, NULL);
    krb5_get_init_creds_opt_set_etype_list( outOptions, etypes, sizeof(etypes)/sizeof(krb5_enctype) );
    krb5_get_init_creds_opt_set_preauth_list(outOptions, preauth, sizeof(preauth)/sizeof(krb5_preauthtype) );
}

typedef void * kbrccache_t;
#define CCACHE_PREFIX_DEFAULT "MEMORY:C_"

static kbrccache_t userinitcontext(
    const char * user, const char * domain, const char * passwd, const char * cachename, int initialize,
    int * outError )
{
    krb5_context    kcontext = 0;
    krb5_ccache             kcache = 0;
    krb5_creds              kcreds;
    krb5_principal  kme = 0;
    krb5_error_code kres;
    char *                  pPass = strdup( passwd );
    char *                  pName = NULL;
    char *                  pCacheName = NULL;
    int                             numCreds = 0;

    memset( &kcreds, 0, sizeof(kcreds) );
    kres = krb5_init_context( &kcontext );
    if( kres )
        goto return_error;
    if( domain )
        kres = krb5_build_principal( kcontext, &kme, strlen(domain), domain, user, (char *) 0 );
    else
        kres = krb5_parse_name( kcontext, user, &kme );
    if( kres )
        goto fail;
    krb5_unparse_name( kcontext, kme, &pName );
    if( cachename )
    {
        if (asprintf(&pCacheName, "%s%s", cachename, pName) < 0)
        {
            kres = KRB5_CC_NOMEM;
            goto fail;
        }
        kres = krb5_cc_resolve( kcontext, pCacheName, &kcache );
        if( kres )
        {
            kres = krb5_cc_resolve( kcontext, CCACHE_PREFIX_DEFAULT, &kcache );
            if( kres == 0 )
                pCacheName = strdup(CCACHE_PREFIX_DEFAULT);
        }
    }
    else
    {
        kres = krb5_cc_default( kcontext, &kcache );
        pCacheName = strdup( krb5_cc_get_name( kcontext, kcache ) );
    }
    if( kres )
    {
        krb5_free_context(kcontext);
        goto return_error;
    }
    if( initialize )
        krb5_cc_initialize( kcontext, kcache, kme );
    if( kres == 0 && user && passwd )
    {
        long timeneeded = time(0L) +TKTTIMELEFT;
        int have_credentials = 0;
        krb5_cc_cursor cc_curs = NULL;
        numCreds = 0;
        if( (kres=krb5_cc_start_seq_get(kcontext, kcache, &cc_curs)) >= 0 )
        {
            while( (kres=krb5_cc_next_cred(kcontext, kcache, &cc_curs, &kcreds))== 0)
            {
                numCreds++;
                if( krb5_principal_compare( kcontext, kme, kcreds.client ) )
                {
                    if( kcreds.ticket_flags & TKT_FLG_INITIAL && kcreds.times.endtime>timeneeded )
                        have_credentials = 1;
                }
                krb5_free_cred_contents( kcontext, &kcreds );
                if( have_credentials )
                    break;
            }
            krb5_cc_end_seq_get( kcontext, kcache, &cc_curs );
        }
        else
        {
            const char * errmsg = error_message(kres);
            fprintf( stderr, "%s user init(%s): %s\n", "setpass", pName, errmsg );
        }
        if( kres != 0 || have_credentials == 0 )
        {
            krb5_get_init_creds_opt *options = NULL;
            kres = krb5_get_init_creds_opt_alloc(kcontext, &options);
            if ( kres == 0 )
            {
                get_init_creds_opt_init(options);
/*
** no valid credentials - get new ones
*/
                kres = krb5_get_init_creds_password( kcontext, &kcreds, kme, pPass,
                                                     NULL /*prompter*/,
                                                     NULL /*data*/,
                                                     0 /*starttime*/,
                                                     0 /*in_tkt_service*/,
                                                     options /*options*/ );
            }
            if( kres == 0 )
            {
                if( numCreds <= 0 )
                    kres = krb5_cc_initialize( kcontext, kcache, kme );
                if( kres == 0 )
                    kres = krb5_cc_store_cred( kcontext, kcache, &kcreds );
                if( kres == 0 )
                    have_credentials = 1;
            }
            krb5_get_init_creds_opt_free(kcontext, options);
        }
#ifdef NOTUSED
        if( have_credentials )
        {
            int mstat;
            kres = gss_krb5_ccache_name( &mstat, pCacheName, NULL );
            if( getenv( ENV_DEBUG_LDAPKERB ) )
                fprintf( stderr, "gss credentials cache set to %s(%d)\n", pCacheName, kres );
        }
#endif
        krb5_cc_close( kcontext, kcache );
    }
fail:
    if( kres )
    {
        const char * errmsg = error_message(kres);
        fprintf( stderr, "%s user init(%s): %s\n", "setpass", pName, errmsg );
    }
    krb5_free_principal( kcontext, kme );
    krb5_free_cred_contents( kcontext, &kcreds );
    if( pName )
        free( pName );
    free(pPass);
    krb5_free_context(kcontext);

return_error:
    if( kres )
    {
        if( pCacheName )
        {
            free(pCacheName);
            pCacheName = NULL;
        }
    }
    if( outError )
        *outError = kres;
    return pCacheName;
}

static int init_creds()
{
    char user[512];
    char * password = NULL;
    int result;

    user[0] = 0;
    result = -1;

    for(;;)
    {
        while( user[0] == 0 )
        {
            int userlen;
            printf( "Username: ");
            fflush(stdout);
            if( fgets( user, sizeof(user), stdin ) == NULL )
                return -1;
            userlen = strlen( user);
            if( userlen < 2 )
                continue;
            user[userlen-1] = 0;    /* get rid of the newline */
            break;
        }
        {
            kbrccache_t usercontext;
            password = getpass( "Password: ");
            if( ! password )
                return -1;
            result = 0;
            usercontext = userinitcontext( user, NULL, password, NULL, 1, &result );
            if( usercontext )
                break;
        }
    }
    return result;
}

int main( int argc, char ** argv )
{
    char * new_password;
    char * new_password2;
    krb5_context    kcontext;
    krb5_error_code kerr;
    krb5_principal  target_principal;


    if( argc < 2 )
    {
        fprintf( stderr, "Usage: setpass user@REALM\n");
        exit(1);
    }

/*
** verify credentials -
*/
    if( verify_creds() )
        init_creds();
    if( verify_creds() )
    {
        fprintf( stderr, "No user credentials available\n");
        exit(1);
    }
/*
** check the principal name -
*/
    krb5_init_context(&kcontext);
    kerr = krb5_parse_name( kcontext, argv[1], &target_principal );

    {
        char * pname = NULL;
        kerr = krb5_unparse_name( kcontext, target_principal, &pname );
        printf( "Changing password for %s:\n", pname);
        fflush( stdout );
        free( pname );
    }
/*
** get the new password -
*/
    for (;;)
    {
        new_password = getpass("Enter new password: ");
        new_password2 = getpass("Verify new password: ");
        if( strcmp( new_password, new_password2 ) == 0)
            break;
        printf("Passwords do not match\n");
        free( new_password );
        free( new_password2 );
    }
/*
** change the password -
*/
    {
        int pw_result;
        krb5_ccache ccache;
        krb5_data       pw_res_string, res_string;

        kerr = krb5_cc_default( kcontext, &ccache );
        if( kerr == 0 )
        {
            kerr = krb5_set_password_using_ccache(kcontext, ccache, new_password, target_principal,
                                                  &pw_result, &pw_res_string, &res_string );
            if( kerr )
                fprintf( stderr, "Failed: %s\n", error_message(kerr) );
            else
            {
                if( pw_result )
                {
                    fprintf( stderr, "Failed(%d)", pw_result );
                    if( pw_res_string.length > 0 )
                        fprintf( stderr, ": %s", pw_res_string.data);
                    if( res_string.length > 0 )
                        fprintf( stderr, " %s", res_string.data);
                    fprintf( stderr, "\n");
                }
            }
        }
    }
    return(0);
}
