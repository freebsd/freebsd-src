/*
 * Copyright 1985, 1986, 1987, 1988 by the Massachusetts Institute
 * of Technology.
 *
 * For copying and distribution information, please see the file
 * <mit-copyright.h>.
 *
 * This routine changes the Kerberos encryption keys for principals,
 * i.e., users or services. 
 */

/*
 * exit returns 	 0 ==> success -1 ==> error 
 */

#include "adm_locl.h"

RCSID("$Id: kdb_edit.c,v 1.28 1999/09/16 20:37:21 assar Exp $");

#ifdef DEBUG
extern  kerb_debug;
#endif

static int     nflag = 0;
static int     debug;

static des_cblock new_key;

static int     i, j;
static int     more;

static char    input_name[ANAME_SZ];
static char    input_instance[INST_SZ];

#define	MAX_PRINCIPAL	10
static Principal principal_data[MAX_PRINCIPAL];

static Principal old_principal;
static Principal default_princ;

static des_cblock master_key;
static des_cblock session_key;
static des_key_schedule master_key_schedule;
static char pw_str[255];
static long master_key_version;

static void
Usage(void)
{
    fprintf(stderr, "Usage: %s [-n]\n", __progname);
    exit(1);
}

static char *
n_gets(char *buf, int size)
{
    char *p;
    char *ret;
    ret = fgets(buf, size, stdin);
  
    if (ret && (p = strchr(buf, '\n')))
	*p = 0;
    return ret;
}


static int
change_principal(void)
{
    static char temp[255];
    int     creating = 0;
    int     editpw = 0;
    int     changed = 0;
    long    temp_long;		/* Don't change to int32_t, used by scanf */
    struct tm 	edate;

    fprintf(stdout, "\nPrincipal name: ");
    fflush(stdout);
    if (!n_gets(input_name, sizeof(input_name)) || *input_name == '\0')
	return 0;
    fprintf(stdout, "Instance: ");
    fflush(stdout);
    /* instance can be null */
    n_gets(input_instance, sizeof(input_instance));
    j = kerb_get_principal(input_name, input_instance, principal_data,
			   MAX_PRINCIPAL, &more);
    if (!j) {
	fprintf(stdout, "\n\07\07<Not found>, Create [y] ? ");
	fflush(stdout);
	n_gets(temp, sizeof(temp));		/* Default case should work, it didn't */
	if (temp[0] != 'y' && temp[0] != 'Y' && temp[0] != '\0')
	    return -1;
	/* make a new principal, fill in defaults */
	j = 1;
	creating = 1;
	strlcpy(principal_data[0].name,
			input_name,
			ANAME_SZ);
	strlcpy(principal_data[0].instance,
			input_instance,
			INST_SZ);
	principal_data[0].old = NULL;
	principal_data[0].exp_date = default_princ.exp_date;
	if (strcmp(input_instance, "admin") == 0)
	  principal_data[0].max_life = 1 + (CLOCK_SKEW/(5*60)); /*5+5 minutes*/
	else if (strcmp(input_instance, "root") == 0)
	  principal_data[0].max_life = 96; /* 8 hours */
	else
	  principal_data[0].max_life = default_princ.max_life;
	principal_data[0].attributes = default_princ.attributes;
	principal_data[0].kdc_key_ver = (unsigned char) master_key_version;
	principal_data[0].key_version = 0; /* bumped up later */
    }
    *principal_data[0].exp_date_txt = '\0';
    for (i = 0; i < j; i++) {
	for (;;) {
	    fprintf(stdout,
		    "\nPrincipal: %s, Instance: %s, kdc_key_ver: %d",
		    principal_data[i].name, principal_data[i].instance,
		    principal_data[i].kdc_key_ver);
	    fflush(stdout);
	    editpw = 1;
	    changed = 0;
	    if (!creating) {
		/*
		 * copy the existing data so we can use the old values
		 * for the qualifier clause of the replace 
		 */
		principal_data[i].old = (char *) &old_principal;
		memcpy(&old_principal, &principal_data[i],
		       sizeof(old_principal));
		printf("\nChange password [n] ? ");
		n_gets(temp, sizeof(temp));
		if (strcmp("y", temp) && strcmp("Y", temp))
		    editpw = 0;
	    }
	    /* password */
	    if (editpw) {
#ifdef NOENCRYPTION
		placebo_read_pw_string(pw_str, sizeof pw_str,
		    "\nNew Password: ", TRUE);
#else
                if(des_read_pw_string(pw_str, sizeof pw_str,
			"\nNew Password: ", TRUE))
		    continue;
#endif
		if (   strcmp(pw_str, "RANDOM") == 0
		    || strcmp(pw_str, "") == 0) {
		    printf("\nRandom password [y] ? ");
		    n_gets(temp, sizeof(temp));
		    if (!strcmp("n", temp) || !strcmp("N", temp)) {
			/* no, use literal */
#ifdef NOENCRYPTION
			memset(new_key, 0, sizeof(des_cblock));
			new_key[0] = 127;
#else
			des_string_to_key(pw_str, &new_key);
#endif
			memset(pw_str, 0, sizeof pw_str);	/* "RANDOM" */
		    } else {
#ifdef NOENCRYPTION
			memset(new_key, 0, sizeof(des_cblock));
			new_key[0] = 127;
#else
			des_new_random_key(&new_key);
#endif
			memset(pw_str, 0, sizeof pw_str);
		    }
		} else if (!strcmp(pw_str, "NULL")) {
		    printf("\nNull Key [y] ? ");
		    n_gets(temp, sizeof(temp));
		    if (!strcmp("n", temp) || !strcmp("N", temp)) {
			/* no, use literal */
#ifdef NOENCRYPTION
			memset(new_key, 0, sizeof(des_cblock));
			new_key[0] = 127;
#else
			des_string_to_key(pw_str, &new_key);
#endif
			memset(pw_str, 0, sizeof pw_str);	/* "NULL" */
		    } else {

			principal_data[i].key_low = 0;
			principal_data[i].key_high = 0;
			goto null_key;
		    }
		} else {
#ifdef NOENCRYPTION
		    memset(new_key, 0, sizeof(des_cblock));
		    new_key[0] = 127;
#else
		    des_string_to_key(pw_str, &new_key);
#endif
		    memset(pw_str, 0, sizeof pw_str);
		}

		/* seal it under the kerberos master key */
		kdb_encrypt_key (&new_key, &new_key, 
				 &master_key, master_key_schedule,
				 DES_ENCRYPT);
		copy_from_key(new_key,
			      &principal_data[i].key_low,
			      &principal_data[i].key_high);
		memset(new_key, 0, sizeof(new_key));
	null_key:
		/* set master key version */
		principal_data[i].kdc_key_ver =
		    (unsigned char) master_key_version;
		/* bump key version # */
		principal_data[i].key_version++;
		fprintf(stdout,
			"\nPrincipal's new key version = %d\n",
			principal_data[i].key_version);
		fflush(stdout);
		changed = 1;
	    }
	    /* expiration date */
	    {
		char d[DATE_SZ];
		struct tm *tm;
		tm = k_localtime(&principal_data[i].exp_date);
		strftime(d, sizeof(d), "%Y-%m-%d", tm);
		while(1) {
		    printf("Expiration date (yyyy-mm-dd) [ %s ] ? ", d);
		    fflush(stdout);
		    if(n_gets(temp, sizeof(temp)) == NULL) {
			printf("Invalid date.\n");
			continue;
		    }
		    if (*temp) {
			memset(&edate, 0, sizeof(edate));
			if (sscanf(temp, "%d-%d-%d", &edate.tm_year,
				   &edate.tm_mon, &edate.tm_mday) != 3) {
			    printf("Invalid date.\n");
			    continue;
			}
			edate.tm_mon--;     /* January is 0, not 1 */
			edate.tm_hour = 23; /* at the end of the */
			edate.tm_min = 59;  /* specified day */
			if (krb_check_tm (edate)) {
			    printf("Invalid date.\n");
			    continue;
			}
			edate.tm_year -= 1900;
			principal_data[i].exp_date = tm2time (edate, 1);
			changed = 1;
		    }
		    break;
		}
	    }

	    /* maximum lifetime */
	    fprintf(stdout, "Max ticket lifetime (*5 minutes) [ %d ] ? ",
		    principal_data[i].max_life);
	    fflush(stdout);
	    while (n_gets(temp, sizeof(temp)) && *temp) {
		if (sscanf(temp, "%ld", &temp_long) != 1)
		    goto bad_life;
		if (temp_long > 255 || (temp_long < 0)) {
		bad_life:
		    fprintf(stdout, "\07\07Invalid, choose 0-255\n");
		    fprintf(stdout,
			    "Max ticket lifetime (*5 minutes) [ %d ] ? ",
			    principal_data[i].max_life);
		    fflush(stdout);
		    continue;
		}
		changed = 1;
		/* dont clobber */
		principal_data[i].max_life = (unsigned short) temp_long;
		break;
	    }

	    /* attributes */
	    fprintf(stdout, "Attributes [ %d ] ? ",
		    principal_data[i].attributes);
	    fflush(stdout);
	    while (n_gets(temp, sizeof(temp)) && *temp) {
		if (sscanf(temp, "%ld", &temp_long) != 1)
		    goto bad_att;
		if (temp_long > 65535 || (temp_long < 0)) {
		bad_att:
		    fprintf(stdout, "Invalid, choose 0-65535\n");
		    fprintf(stdout, "Attributes [ %d ] ? ",
			    principal_data[i].attributes);
		    fflush(stdout);
		    continue;
		}
		changed = 1;
		/* dont clobber */
		principal_data[i].attributes =
		    (unsigned short) temp_long;
		break;
	    }

	    /*
	     * remaining fields -- key versions and mod info, should
	     * not be directly manipulated 
	     */
	    if (changed) {
		if (kerb_put_principal(&principal_data[i], 1)) {
		    fprintf(stdout,
			"\nError updating Kerberos database");
		} else {
		    fprintf(stdout, "Edit O.K.");
		}
	    } else {
		fprintf(stdout, "Unchanged");
	    }


	    memset(&principal_data[i].key_low, 0, 4);
	    memset(&principal_data[i].key_high, 0, 4);
	    fflush(stdout);
	    break;
	}
    }
    if (more) {
	fprintf(stdout, "\nThere were more tuples found ");
	fprintf(stdout, "than there were space for");
      }
    return 1;
}

static void
cleanup(void)
{

    memset(master_key, 0, sizeof(master_key));
    memset(session_key, 0, sizeof(session_key));
    memset(master_key_schedule, 0, sizeof(master_key_schedule));
    memset(principal_data, 0, sizeof(principal_data));
    memset(new_key, 0, sizeof(new_key));
    memset(pw_str, 0, sizeof(pw_str));
}

int
main(int argc, char **argv)
{
    /* Local Declarations */

    long    n;

    set_progname (argv[0]);

    while (--argc > 0 && (*++argv)[0] == '-')
	for (i = 1; argv[0][i] != '\0'; i++) {
	    switch (argv[0][i]) {

		/* debug flag */
	    case 'd':
		debug = 1;
		continue;

		/* debug flag */
#ifdef DEBUG
	    case 'l':
		kerb_debug |= 1;
		continue;
#endif
	    case 'n':		/* read MKEYFILE for master key */
		nflag = 1;
		continue;

	    default:
		warnx ("illegal flag \"%c\"", argv[0][i]);
		Usage();	/* Give message and die */
	    }
	}

    fprintf(stdout, "Opening database...\n");
    fflush(stdout);
    kerb_init();
    if (argc > 0)
	if (kerb_db_set_name(*argv) != 0)
	    errx (1, "Could not open altername database name");

    if (kdb_get_master_key ((nflag == 0) ? KDB_GET_PROMPT : 0, 
			    &master_key, master_key_schedule) != 0)
	errx (1, "Couldn't read master key.");

    if ((master_key_version = kdb_verify_master_key(&master_key,
						    master_key_schedule,
						    stdout)) < 0)
      return 1;

    /* Initialize non shared random sequence */
    des_init_random_number_generator(&master_key);

    /* lookup the default values */
    n = kerb_get_principal(KERB_DEFAULT_NAME, KERB_DEFAULT_INST,
			   &default_princ, 1, &more);
    if (n != 1)
	errx (1, "Kerberos error on default value lookup, %ld found.", n);
    fprintf(stdout, "Previous or default values are in [brackets] ,\n");
    fprintf(stdout, "enter return to leave the same, or new value.\n");

    while (change_principal()) {
    }

    cleanup();
    return 0;
}
