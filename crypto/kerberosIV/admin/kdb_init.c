/*
 * Copyright 1987, 1988 by the Massachusetts Institute of Technology. 
 *
 * For copying and distribution information, please see the file
 * <mit-copyright.h>. 
 *
 * program to initialize the database,  reports error if database file
 * already exists. 
 */

#include "adm_locl.h"

RCSID("$Id: kdb_init.c,v 1.23 1997/03/30 17:45:05 assar Exp $");

enum ap_op {
    NULL_KEY,			/* setup null keys */
    MASTER_KEY,                 /* use master key as new key */
    RANDOM_KEY			/* choose a random key */
};

static des_cblock master_key;
static des_key_schedule master_key_schedule;

/* use a return code to indicate success or failure.  check the return */
/* values of the routines called by this routine. */

static int
add_principal(char *name, char *instance, enum ap_op aap_op, int maxlife)
{
    Principal principal;
    struct tm *tm;
    des_cblock new_key;

    memset(&principal, 0, sizeof(principal));
    strncpy(principal.name, name, ANAME_SZ);
    strncpy(principal.instance, instance, INST_SZ);
    switch (aap_op) {
    case NULL_KEY:
	principal.key_low = 0;
	principal.key_high = 0;
	break;
    case RANDOM_KEY:
#ifdef NOENCRYPTION
        memset(new_key, 0, sizeof(des_cblock));
	new_key[0] = 127;
#else
	des_new_random_key(&new_key);
#endif
	kdb_encrypt_key (&new_key, &new_key, &master_key, master_key_schedule,
			 DES_ENCRYPT);
	copy_from_key(new_key, &principal.key_low, &principal.key_high);
	memset(new_key, 0, sizeof(new_key));
	break;
    case MASTER_KEY:
	memcpy(new_key, master_key, sizeof (des_cblock));
	kdb_encrypt_key (&new_key, &new_key, &master_key, master_key_schedule,
			 DES_ENCRYPT);
	copy_from_key(new_key, &principal.key_low, &principal.key_high);
	break;
    }
    principal.exp_date = 946702799;	/* Happy new century */
    strncpy(principal.exp_date_txt, "12/31/99", DATE_SZ);
    principal.mod_date = time(0);

    tm = k_localtime(&principal.mod_date);
    principal.attributes = 0;
    principal.max_life = maxlife;

    principal.kdc_key_ver = 1;
    principal.key_version = 1;

    strncpy(principal.mod_name, "db_creation", ANAME_SZ);
    strncpy(principal.mod_instance, "", INST_SZ);
    principal.old = 0;

    if (kerb_db_put_principal(&principal, 1) != 1)
        return -1;		/* FAIL */
    
    /* let's play it safe */
    memset(new_key, 0, sizeof (des_cblock));
    memset(&principal.key_low, 0, 4);
    memset(&principal.key_high, 0, 4);
    return 0;
}

int
main(int argc, char **argv)
{
    char    realm[REALM_SZ];
    char   *cp;
    int code;
    char *database;
    
    set_progname (argv[0]);

    if (argc > 3) {
	fprintf(stderr, "Usage: %s [realm-name] [database-name]\n", argv[0]);
	return 1;
    }
    if (argc == 3) {
	database = argv[2];
	--argc;
    } else
	database = DBM_FILE;

    /* Do this first, it'll fail if the database exists */
    if ((code = kerb_db_create(database)) != 0)
	err (1, "Couldn't create database %s", database);
    kerb_db_set_name(database);

    if (argc == 2)
	strncpy(realm, argv[1], REALM_SZ);
    else {
        if (krb_get_lrealm(realm, 1) != KSUCCESS)
		strcpy(realm, KRB_REALM);
	fprintf(stderr, "Realm name [default  %s ]: ", realm);
	if (fgets(realm, sizeof(realm), stdin) == NULL)
	    errx (1, "\nEOF reading realm");
	if ((cp = strchr(realm, '\n')))
	    *cp = '\0';
	if (!*realm)			/* no realm given */
		if (krb_get_lrealm(realm, 1) != KSUCCESS)
			strcpy(realm, KRB_REALM);
    }
    if (!k_isrealm(realm))
	errx (1, "Bad kerberos realm name \"%s\"", realm);
#ifndef RANDOM_MKEY
    printf("You will be prompted for the database Master Password.\n");
    printf("It is important that you NOT FORGET this password.\n");
#else
    printf("To generate a master key, please enter some random data.\n");
    printf("You do not have to remember this.\n");
#endif
    fflush(stdout);

    if (kdb_get_master_key (KDB_GET_TWICE, &master_key,
			    master_key_schedule) != 0)
	errx (1, "Couldn't read master key.");

#ifdef RANDOM_MKEY
    if(kdb_kstash(&master_key, MKEYFILE) < 0)
	err (1, "Error writing master key");
    fprintf(stderr, "Wrote master key to %s\n", MKEYFILE);
#endif

    /* Initialize non shared random sequence */
    des_init_random_number_generator(&master_key);

    /* Maximum lifetime for changepw.kerberos (kadmin) tickets, 10 minutes */
#define ADMLIFE (1 + (CLOCK_SKEW/(5*60)))

    /* Maximum lifetime for ticket granting tickets, 4 days or 21.25h */
#define TGTLIFE ((krb_life_to_time(0, 162) >= 24*60*60) ? 161 : 255)

    /* This means that default lifetimes have not been initialized */
#define DEFLIFE 255

#define NOLIFE 0

    if (
	add_principal(KERB_M_NAME, KERB_M_INST, MASTER_KEY, NOLIFE) ||
	add_principal(KERB_DEFAULT_NAME, KERB_DEFAULT_INST, NULL_KEY,DEFLIFE)||
	add_principal(KRB_TICKET_GRANTING_TICKET, realm, RANDOM_KEY, TGTLIFE)||
	add_principal(PWSERV_NAME, KRB_MASTER, RANDOM_KEY, ADMLIFE) 
	) {
      putc ('\n', stderr);
      errx (1, "couldn't initialize database.");
    }

    /* play it safe */
    memset(master_key, 0, sizeof (des_cblock));
    memset(master_key_schedule, 0, sizeof (des_key_schedule));
    return 0;
}
