/* tests/hammer/kdc5_hammer.c */
/*
 * Copyright 1990,1991 by the Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

#include "k5-int.h"
#include "com_err.h"
#include <sys/time.h>

#define KRB5_DEFAULT_OPTIONS 0
#define KRB5_DEFAULT_LIFE 60*60*8 /* 8 hours */
#define KRB5_RENEWABLE_LIFE 60*60*2 /* 2 hours */

struct h_timer {
    float	ht_cumulative;
    float	ht_min;
    float	ht_max;
    krb5_int32	ht_observations;
};

extern int optind;
extern char *optarg;
char *prog;

static int brief;
static char *cur_realm = 0;
static int do_timer = 0;

krb5_data tgtname = {
    0,
    KRB5_TGS_NAME_SIZE,
    KRB5_TGS_NAME
};

int verify_cs_pair
	(krb5_context,
		   char *,
		   krb5_principal,
		   char *,
		   char *,
		   int, int, int,
		   krb5_ccache);

int get_tgt
	(krb5_context,
		   char *,
		   krb5_principal *,
		   krb5_ccache);

static void
usage(who, status)
char *who;
int status;
{
    fprintf(stderr,
	    "usage: %s -p prefix -n num_to_check [-c cachename] [-r realmname]\n",
	    who);
    fprintf(stderr, "\t [-D depth]\n");
    fprintf(stderr, "\t [-P preauth type] [-R repeat_count] [-t] [-b] [-v] \n");

    exit(status);
}

static krb5_preauthtype * patype = NULL, patypedata[2] = { 0, -1 };
static krb5_context test_context;

struct timeval	tstart_time, tend_time;
struct timezone	dontcare;

struct h_timer in_tkt_times = { 0.0, 1000000.0, -1.0, 0 };
struct h_timer tgs_req_times = { 0.0, 1000000.0, -1.0, 0 };
/*
 * Timer macros.
 */
#define	swatch_on()	((void) gettimeofday(&tstart_time, &dontcare))
#define	swatch_eltime()	((gettimeofday(&tend_time, &dontcare)) ? -1.0 :	\
			 (((float) (tend_time.tv_sec -			\
				    tstart_time.tv_sec)) +		\
			  (((float) (tend_time.tv_usec -		\
				     tstart_time.tv_usec))/1000000.0)))

int
main(argc, argv)
    int argc;
    char **argv;
{
    krb5_ccache ccache = NULL;
    char *cache_name = NULL;		/* -f option */
    int option;
    int errflg = 0;
    krb5_error_code code;
    int num_to_check, n, i, j, repeat_count, counter;
    int n_tried, errors;
    char prefix[BUFSIZ], client[4096], server[4096];
    int depth;
    char ctmp[4096], ctmp2[BUFSIZ], stmp[4096], stmp2[BUFSIZ];
    krb5_principal client_princ;
    krb5_error_code retval;

    krb5_init_context(&test_context);

    if (strrchr(argv[0], '/'))
	prog = strrchr(argv[0], '/')+1;
    else
	prog = argv[0];

    num_to_check = 0;
    depth = 1;
    repeat_count = 1;
    brief = 0;
    n_tried = 0;
    errors = 0;

    while ((option = getopt(argc, argv, "D:p:n:c:R:P:e:bvr:t")) != -1) {
	switch (option) {
	case 't':
	    do_timer = 1;
	    break;
	case 'b':
	    brief = 1;
	    break;
	case 'v':
	    brief = 0;
	    break;
	case 'R':
	    repeat_count = atoi(optarg); /* how many times? */
	    break;
	case 'r':
	    cur_realm = optarg;
	    break;
	case 'D':
	    depth = atoi(optarg);       /* how deep to go */
	    break;
	case 'p':                       /* prefix name to check */
	    strncpy(prefix, optarg, sizeof(prefix) - 1);
	    prefix[sizeof(prefix) - 1] = '\0';
	    break;
       case 'n':                        /* how many to check */
	    num_to_check = atoi(optarg);
	    break;
	case 'P':
	    patypedata[0] = atoi(optarg);
	    patype = patypedata;
	    break;
	case 'c':
	    if (ccache == NULL) {
		cache_name = optarg;

		code = krb5_cc_resolve (test_context, cache_name, &ccache);
		if (code != 0) {
		    com_err (prog, code, "resolving %s", cache_name);
		    errflg++;
		}
	    } else {
		fprintf(stderr, "Only one -c option allowed\n");
		errflg++;
	    }
	    break;
	case '?':
	default:
	    errflg++;
	    break;
	}
    }

    if (!(num_to_check && prefix[0])) usage(prog, 1);

    if (!cur_realm) {
	if ((retval = krb5_get_default_realm(test_context, &cur_realm))) {
	    com_err(prog, retval, "while retrieving default realm name");
	    exit(1);
	}
    }

    if (ccache == NULL) {
	if ((code = krb5_cc_default(test_context, &ccache))) {
	    com_err(prog, code, "while getting default ccache");
	    exit(1);
	}
    }

    memset(ctmp, 0, sizeof(ctmp));
    memset(stmp, 0, sizeof(stmp));

    for (counter = 0; counter < repeat_count; counter++) {
      fprintf(stderr, "\nRound %d\n", counter);

      for (n = 1; n <= num_to_check; n++) {
	/* build the new principal name */
	/* we can't pick random names because we need to generate all the names
	   again given a prefix and count to test the db lib and kdb */
	ctmp[0] = '\0';
	for (i = 1; i <= depth; i++) {
	  (void) snprintf(ctmp2, sizeof(ctmp2), "%s%s%d-DEPTH-%d",
			  (i != 1) ? "/" : "", prefix, n, i);
	  ctmp2[sizeof(ctmp2) - 1] = '\0';
	  strncat(ctmp, ctmp2, sizeof(ctmp) - 1 - strlen(ctmp));
	  ctmp[sizeof(ctmp) - 1] = '\0';
	  snprintf(client, sizeof(client), "%s@%s", ctmp, cur_realm);

	  if (get_tgt (test_context, client, &client_princ, ccache)) {
	    errors++;
	    n_tried++;
	    continue;
	  }
	  n_tried++;

	  stmp[0] = '\0';
	  for (j = 1; j <= depth; j++) {
	    (void) snprintf(stmp2, sizeof(stmp2), "%s%s%d-DEPTH-%d",
			    (j != 1) ? "/" : "", prefix, n, j);
	    stmp2[sizeof (stmp2) - 1] = '\0';
	    strncat(stmp, stmp2, sizeof(stmp) - 1 - strlen(stmp));
	    stmp[sizeof(stmp) - 1] = '\0';
	    snprintf(server, sizeof(server), "%s@%s", stmp, cur_realm);
	    if (verify_cs_pair(test_context, client, client_princ,
			       stmp, cur_realm, n, i, j, ccache))
	      errors++;
	    n_tried++;
	  }
	  krb5_free_principal(test_context, client_princ);
	}
      }
    }
    fprintf (stderr, "\nTried %d.  Got %d errors.\n", n_tried, errors);
    if (do_timer) {
	if (in_tkt_times.ht_observations)
	    fprintf(stderr,
		    "%8d  AS_REQ requests: %9.6f average (min: %9.6f, max:%9.6f)\n",
		    in_tkt_times.ht_observations,
		    in_tkt_times.ht_cumulative /
		    (float) in_tkt_times.ht_observations,
		    in_tkt_times.ht_min,
		    in_tkt_times.ht_max);
	if (tgs_req_times.ht_observations)
	    fprintf(stderr,
		    "%8d TGS_REQ requests: %9.6f average (min: %9.6f, max:%9.6f)\n",
		    tgs_req_times.ht_observations,
		    tgs_req_times.ht_cumulative /
		    (float) tgs_req_times.ht_observations,
		    tgs_req_times.ht_min,
		    tgs_req_times.ht_max);
    }

    (void) krb5_cc_close(test_context, ccache);

    krb5_free_context(test_context);

    exit(errors);
  }


static krb5_error_code
get_server_key(context, server, enctype, key)
    krb5_context context;
    krb5_principal server;
    krb5_enctype enctype;
    krb5_keyblock ** key;
{
    krb5_error_code retval;
    krb5_encrypt_block eblock;
    char * string;
    krb5_data salt;
    krb5_data pwd;

    *key = NULL;

    if ((retval = krb5_principal2salt(context, server, &salt)))
	return retval;

    if ((retval = krb5_unparse_name(context, server, &string)))
	goto cleanup_salt;

    pwd.data = string;
    pwd.length = strlen(string);

    if ((*key = (krb5_keyblock *)malloc(sizeof(krb5_keyblock)))) {
    	krb5_use_enctype(context, &eblock, enctype);
	retval = krb5_string_to_key(context, &eblock, *key, &pwd, &salt);
	if (retval) {
	    free(*key);
	    *key = NULL;
	}
    } else
        retval = ENOMEM;

    free(string);

cleanup_salt:
    free(salt.data);
    return retval;
}

int verify_cs_pair(context, p_client_str, p_client, service, hostname,
		   p_num, c_depth, s_depth, ccache)
    krb5_context context;
    char *p_client_str;
    krb5_principal p_client;
    char * service;
    char * hostname;
    int p_num, c_depth, s_depth;
    krb5_ccache ccache;
{
    krb5_error_code 	  retval;
    krb5_creds 	 	  creds;
    krb5_creds 		* credsp = NULL;
    krb5_ticket 	* ticket = NULL;
    krb5_keyblock 	* keyblock = NULL;
    krb5_auth_context 	  auth_context = NULL;
    krb5_data		  request_data = empty_data();
    char		* sname;
    float		  dt;

    if (brief)
      fprintf(stderr, "\tprinc (%d) client (%d) for server (%d)\n",
	      p_num, c_depth, s_depth);
    else
      fprintf(stderr, "\tclient %s for server %s\n", p_client_str,
	      service);

    /* Initialize variables */
    memset(&creds, 0, sizeof(creds));

    /* Do client side */
    if (asprintf(&sname, "%s@%s", service, hostname) >= 0) {
	retval = krb5_parse_name(context, sname, &creds.server);
	free(sname);
    }
    else
	retval = ENOMEM;
    if (retval)
	return(retval);

    /* obtain ticket & session key */
    if ((retval = krb5_cc_get_principal(context, ccache, &creds.client))) {
	com_err(prog, retval, "while getting client princ for %s", hostname);
	return retval;
    }

    if ((retval = krb5_get_credentials(context, 0,
                                      ccache, &creds, &credsp))) {
	com_err(prog, retval, "while getting creds for %s", hostname);
	return retval;
    }

    if (do_timer)
	swatch_on();

    if ((retval = krb5_mk_req_extended(context, &auth_context, 0, NULL,
			            credsp, &request_data))) {
	com_err(prog, retval, "while preparing AP_REQ for %s", hostname);
	goto cleanup;
    }

    krb5_auth_con_free(context, auth_context);
    auth_context = NULL;

    /* Do server side now */
    if ((retval = get_server_key(context, credsp->server,
				credsp->keyblock.enctype, &keyblock))) {
	com_err(prog, retval, "while getting server key for %s", hostname);
	goto cleanup;
    }

    if (krb5_auth_con_init(context, &auth_context)) {
	com_err(prog, retval, "while creating auth_context for %s", hostname);
	goto cleanup;
    }

    if (krb5_auth_con_setuseruserkey(context, auth_context, keyblock)) {
	com_err(prog, retval, "while setting auth_context key %s", hostname);
	goto cleanup;
    }

    if ((retval = krb5_rd_req(context, &auth_context, &request_data,
			     NULL /* server */, 0, NULL, &ticket))) {
	com_err(prog, retval, "while decoding AP_REQ for %s", hostname);
	goto cleanup;
    }

    if (do_timer) {
	dt = swatch_eltime();
	tgs_req_times.ht_cumulative += dt;
	tgs_req_times.ht_observations++;
	if (dt > tgs_req_times.ht_max)
	    tgs_req_times.ht_max = dt;
	if (dt < tgs_req_times.ht_min)
	    tgs_req_times.ht_min = dt;
    }

    if (!(krb5_principal_compare(context,ticket->enc_part2->client,p_client))){
    	char *returned_client;
        if ((retval = krb5_unparse_name(context, ticket->enc_part2->client,
			       	       &returned_client)))
	    com_err (prog, retval,
		     "Client not as expected, but cannot unparse client name");
      	else
	    com_err (prog, 0, "Client not as expected (%s).", returned_client);
	retval = KRB5_PRINC_NOMATCH;
      	free(returned_client);
    } else {
	retval = 0;
    }

cleanup:
    krb5_free_cred_contents(context, &creds);
    krb5_free_ticket(context, ticket);
    krb5_auth_con_free(context, auth_context);
    krb5_free_keyblock(context, keyblock);
    krb5_free_data_contents(context, &request_data);
    krb5_free_creds(context, credsp);

    return retval;
}

int get_tgt (context, p_client_str, p_client, ccache)
    krb5_context context;
    char *p_client_str;
    krb5_principal *p_client;
    krb5_ccache ccache;
{
    long lifetime = KRB5_DEFAULT_LIFE;	/* -l option */
    krb5_error_code code;
    krb5_creds my_creds;
    krb5_timestamp start;
    float dt;
    krb5_get_init_creds_opt *options;

    if (!brief)
      fprintf(stderr, "\tgetting TGT for %s\n", p_client_str);

    if ((code = krb5_timeofday(context, &start))) {
	com_err(prog, code, "while getting time of day");
	return(-1);
    }

    memset(&my_creds, 0, sizeof(my_creds));

    if ((code = krb5_parse_name (context, p_client_str, p_client))) {
	com_err (prog, code, "when parsing name %s", p_client_str);
	return(-1);
    }

    code = krb5_cc_initialize (context, ccache, *p_client);
    if (code != 0) {
	com_err (prog, code, "when initializing cache");
	return(-1);
    }

    if (do_timer)
	swatch_on();

    code = krb5_get_init_creds_opt_alloc(context, &options);
    if (code != 0) {
	com_err(prog, code, "when allocating init cred options");
	return(-1);
    }

    krb5_get_init_creds_opt_set_tkt_life(options, lifetime);

    code = krb5_get_init_creds_opt_set_out_ccache(context, options, ccache);
    if (code != 0) {
	com_err(prog, code, "when setting init cred output ccache");
	return(-1);
    }

    code = krb5_get_init_creds_password(context, &my_creds, *p_client,
					p_client_str, NULL, NULL, 0, NULL,
					options);
    if (do_timer) {
	dt = swatch_eltime();
	in_tkt_times.ht_cumulative += dt;
	in_tkt_times.ht_observations++;
	if (dt > in_tkt_times.ht_max)
	    in_tkt_times.ht_max = dt;
	if (dt < in_tkt_times.ht_min)
	    in_tkt_times.ht_min = dt;
    }
    krb5_get_init_creds_opt_free(context, options);
    krb5_free_cred_contents(context, &my_creds);
    if (code != 0) {
	com_err (prog, code, "while getting initial credentials");
	return(-1);
      }

    return(0);
}
