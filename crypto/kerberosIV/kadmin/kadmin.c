/* 
  Copyright (C) 1989 by the Massachusetts Institute of Technology

   Export of this software from the United States of America is assumed
   to require a specific license from the United States Government.
   It is the responsibility of any person or organization contemplating
   export to obtain such a license before exporting.

WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
distribute this software and its documentation for any purpose and
without fee is hereby granted, provided that the above copyright
notice appear in all copies and that both that copyright notice and
this permission notice appear in supporting documentation, and that
the name of M.I.T. not be used in advertising or publicity pertaining
to distribution of the software without specific, written prior
permission.  M.I.T. makes no representations about the suitability of
this software for any purpose.  It is provided "as is" without express
or implied warranty.

  */

/*
 * Kerberos database administrator's tool.  
 * 
 * The default behavior of kadmin is if the -m option is given 
 * on the commandline, multiple requests are allowed to be given
 * with one entry of the admin password (until the tickets expire).
 */

#include "kadm_locl.h"

RCSID("$Id: kadmin.c,v 1.48 1997/05/13 09:43:06 bg Exp $");

static void change_password(int argc, char **argv);
static void change_key(int argc, char **argv);
static void change_admin_password(int argc, char **argv);
static void add_new_key(int argc, char **argv);
static void del_entry(int argc, char **argv);
static void get_entry(int argc, char **argv);
static void mod_entry(int argc, char **argv);
static void help(int argc, char **argv);
static void clean_up_cmd(int argc, char **argv);
static void quit_cmd(int argc, char **argv);

static SL_cmd cmds[] = {
  {"change_password", change_password, "Change a user's password"},
  {"cpw"},
  {"passwd"},
  {"change_key", change_key, "Change a user's password as a DES binary key"},
  {"ckey"},
  {"change_admin_password", change_admin_password,
   "Change your admin password"},
  {"cap"},
  {"add_new_key", add_new_key, "Add new user to kerberos database"},
  {"ank"},
  {"del_entry", del_entry, "Delete entry from database"},
  {"del"},
  {"delete"},
  {"get_entry", get_entry, "Get entry from kerberos database"},
  {"mod_entry", mod_entry, "Modify entry in kerberos database"},
  {"destroy_tickets", clean_up_cmd, "Destroy admin tickets"},
  {"exit", quit_cmd, "Exit program"},
  {"quit"},
  {"help", help, "Help"},
  {"?"},
  {NULL}
};

#define BAD_PW 1
#define GOOD_PW 0
#define FUDGE_VALUE 15		/* for ticket expiration time */
#define PE_NO 0
#define PE_YES 1
#define PE_UNSURE 2

/* for get_password, whether it should do the swapping...necessary for
   using vals structure, unnecessary for change_pw requests */
#define DONTSWAP 0
#define SWAP 1

static krb_principal pr;
static char default_realm[REALM_SZ]; /* default kerberos realm */
static char krbrlm[REALM_SZ];	/* current realm being administered */
static int multiple = 0;	/* Allow multiple requests per ticket */

#ifdef NOENCRYPTION
#define read_long_pw_string placebo_read_pw_string
#else
#define read_long_pw_string des_read_pw_string
#endif

static void
get_maxlife(Kadm_vals *vals)
{
    char buff[BUFSIZ];
    time_t life;
    int l;

    do {
	printf("Maximum ticket lifetime?  (%d)  [%s]  ",
 	     vals->max_life, krb_life_to_atime(vals->max_life));
	fflush(stdout);
	if (fgets(buff, sizeof(buff), stdin) == NULL || *buff == '\n') {
	    clearerr(stdin);
	    return;
	}
	life = krb_atime_to_life(buff);
    } while (life <= 0);

    l = strlen(buff);
    if (buff[l-2] == 'm')
	life = krb_time_to_life(0L, life*60);
    if (buff[l-2] == 'h')
	life = krb_time_to_life(0L, life*60*60);

    vals->max_life = life;
    SET_FIELD(KADM_MAXLIFE,vals->fields);
}

static void
get_attr(Kadm_vals *vals)
{
    char buff[BUFSIZ], *out;
    int attr;

    do {
	printf("Attributes?  [0x%.2x]  ", vals->attributes);
	fflush(stdout);
	if (fgets(buff, sizeof(buff), stdin) == NULL || *buff == '\n') {
	    clearerr(stdin);
	    return;
	}
        attr = strtol(buff, &out, 0);
	if (attr == 0 && out == buff)
	  attr = -1;
    } while (attr < 0 || attr > 0xffff);

    vals->attributes = attr;
    SET_FIELD(KADM_ATTR,vals->fields);
}

static void
get_expdate(Kadm_vals *vals)
{
    char buff[BUFSIZ];
    struct tm edate;

    memset(&edate, 0, sizeof(edate));
    do {
        printf("Expiration date (enter yyyy-mm-dd) ?  [%.24s]  ",
             asctime(k_localtime(&vals->exp_date)));
        fflush(stdout);
        if (fgets(buff, sizeof(buff), stdin) == NULL || *buff == '\n') {
            clearerr(stdin);
            return;
        }
        if (sscanf(buff, "%d-%d-%d",
                   &edate.tm_year, &edate.tm_mon, &edate.tm_mday) == 3) {
            edate.tm_mon--;     /* January is 0, not 1 */
            edate.tm_hour = 23; /* nearly midnight at the end of the */
            edate.tm_min = 59;  /* specified day */
        }
    } while (krb_check_tm (edate));

    edate.tm_year -= 1900;
    vals->exp_date = tm2time (edate, 1);
    SET_FIELD(KADM_EXPDATE,vals->fields);
}

static int
princ_exists(char *name, char *instance, char *realm)
{
    int status;

    status = krb_get_pw_in_tkt(name, instance, realm,
			       KRB_TICKET_GRANTING_TICKET,
			       realm, 1, "");

    if ((status == KSUCCESS) || (status == INTK_BADPW))
	return(PE_YES);
    else if (status == KDC_PR_UNKNOWN)
	return(PE_NO);
    else
	return(PE_UNSURE);
}

static int
get_password(u_int32_t *low, u_int32_t *high, char *prompt, int byteswap)
{
    char new_passwd[MAX_KPW_LEN];	/* new password */
    des_cblock newkey;

    if (read_long_pw_string(new_passwd, sizeof(new_passwd)-1, prompt, 1))
    	return(BAD_PW);
    if (strlen(new_passwd) == 0) {
    	printf("Using random password.\n");
#ifdef NOENCRYPTION
	memset(newkey, 0, sizeof(newkey));
#else
	des_new_random_key(&newkey);
#endif
    } else {
#ifdef NOENCRYPTION
      memset(newkey, 0, sizeof(newkey));
#else
      des_string_to_key(new_passwd, &newkey);
#endif
      memset(new_passwd, 0, sizeof(new_passwd));
    }

    memcpy(low, newkey, 4);
    memcpy(high, ((char *)newkey) + 4, 4);

    memset(newkey, 0, sizeof(newkey));

#ifdef NOENCRYPTION
    *low = 1;
#endif

    if (byteswap != DONTSWAP) {
	*low = htonl(*low);
	*high = htonl(*high);
    }
    return(GOOD_PW);
}

static int
get_admin_password(void)
{
    int status;
    char admin_passwd[MAX_KPW_LEN];	/* Admin's password */
    int ticket_life = 1;	/* minimum ticket lifetime */
    CREDENTIALS c;

    if (multiple) {
	/* If admin tickets exist and are valid, just exit. */
	memset(&c, 0, sizeof(c));
	if (krb_get_cred(PWSERV_NAME, KADM_SINST, krbrlm, &c) == KSUCCESS)
	    /* 
	     * If time is less than lifetime - FUDGE_VALUE after issue date,
	     * tickets will probably last long enough for the next 
	     * transaction.
	     */
	    if (time(0) < (c.issue_date + (5 * 60 * c.lifetime) - FUDGE_VALUE))
		return(KADM_SUCCESS);
	ticket_life = DEFAULT_TKT_LIFE;
    }
    
    if (princ_exists(pr.name, pr.instance, pr.realm) != PE_NO) {
        char prompt[256];
	snprintf(prompt, sizeof(prompt), "%s's Password: ", krb_unparse_name(&pr));
	if (read_long_pw_string(admin_passwd,
				sizeof(admin_passwd)-1,
				prompt, 0)) {
	    warnx ("Error reading admin password.");
	    goto bad;
	}
	status = krb_get_pw_in_tkt(pr.name, pr.instance, pr.realm,
				   PWSERV_NAME, KADM_SINST,
				   ticket_life, admin_passwd);
	memset(admin_passwd, 0, sizeof(admin_passwd));

	/* Initialize non shared random sequence from session key. */
	memset(&c, 0, sizeof(c));
	krb_get_cred(PWSERV_NAME, KADM_SINST, krbrlm, &c);
	des_init_random_number_generator(&c.session);
    }
    else
	status = KDC_PR_UNKNOWN;

    switch(status) {
    case GT_PW_OK:
	return(GOOD_PW);
    case KDC_PR_UNKNOWN:
	printf("Principal %s does not exist.\n", krb_unparse_name(&pr));
	goto bad;
    case GT_PW_BADPW:
	printf("Incorrect admin password.\n");
	goto bad;
    default:
	com_err("kadmin", status+krb_err_base,
		"while getting password tickets");
	goto bad;
    }
    
 bad:
    memset(admin_passwd, 0, sizeof(admin_passwd));
    dest_tkt();
    return(BAD_PW);
}

static void
usage(void)
{
    fprintf (stderr, "Usage: kadmin [[-u|-p] admin_name] [-r default_realm]"
	     " [-m]\n"
	     "   -m allows multiple admin requests to be "
	     "serviced with one entry of admin\n"	     
	     "   password.\n");
    exit (1);
}

/* GLOBAL */
static void
clean_up()
{
    dest_tkt();
}

static void
clean_up_cmd (int argc, char **argv)
{
    clean_up();
}

/* GLOBAL */
static void 
quit()
{
    printf("Cleaning up and exiting.\n");
    clean_up();
    exit(0);
}

static void
quit_cmd (int argc, char **argv)
{
    quit();
}

static void
do_init(int argc, char **argv)
{
    int c;
    int tflag = 0;
    char tktstring[MaxPathLen];
    int k_errno;
    
    set_progname (argv[0]);

    memset(&pr, 0, sizeof(pr));
    if (krb_get_default_principal(pr.name, pr.instance, default_realm) < 0)
	errx (1, "I could not even guess who you might be");
    while ((c = getopt(argc, argv, "p:u:r:mt")) != EOF) 
	switch (c) {
	case 'p':
	case 'u':
	    if((k_errno = krb_parse_name(optarg, &pr)) != KSUCCESS)
		errx (1, "%s", krb_get_err_text(k_errno));
	    break;
	case 'r':
	    memset(default_realm, 0, sizeof(default_realm));
	    strncpy(default_realm, optarg, sizeof(default_realm) - 1);
	    break;
	case 'm':
	    multiple++;
	    break;
	case 't':
	    tflag++;
	    break;
	default:
	    usage();
	    break;
	}
    if (optind < argc)
	usage();
    strcpy(krbrlm, default_realm);

    if (kadm_init_link(PWSERV_NAME, KRB_MASTER, krbrlm) != KADM_SUCCESS)
	*krbrlm = '\0';
    if (pr.realm[0] == '\0')
	strcpy (pr.realm, krbrlm);
    if (pr.instance[0] == '\0')
	strcpy(pr.instance, "admin");
    
    if (!tflag) {
	snprintf(tktstring, sizeof(tktstring), TKT_ROOT "_adm_%d",(int)getpid());
	krb_set_tkt_string(tktstring);
    }
    
}

int
main(int argc, char **argv)
{
    do_init(argc, argv);

    printf("Welcome to the Kerberos Administration Program, version 2\n");
    printf("Type \"help\" if you need it.\n");
    sl_loop (cmds, "kadmin: ");
    printf("\n");
    quit();
    exit(0);
}

static int
setvals(Kadm_vals *vals, char *string)
{
    char realm[REALM_SZ];
    int status = KADM_SUCCESS;

    memset(vals, 0, sizeof(*vals));
    memset(realm, 0, sizeof(realm));

    SET_FIELD(KADM_NAME,vals->fields);
    SET_FIELD(KADM_INST,vals->fields);
    if ((status = kname_parse(vals->name, vals->instance, realm, string))) {
	printf("kerberos error: %s\n", krb_get_err_text(status));
	return status;
    }
    if (!realm[0])
	strcpy(realm, default_realm);
    if (strcmp(realm, krbrlm)) {
	strcpy(krbrlm, realm);
	if ((status = kadm_init_link(PWSERV_NAME, KRB_MASTER, krbrlm)) 
	    != KADM_SUCCESS)
	    printf("kadm error for realm %s: %s\n", 
		   krbrlm, error_message(status));
    }
    if (status) 
	return 1;
    else
	return KADM_SUCCESS;
}    

static void 
change_password(int argc, char **argv)
{
    Kadm_vals old, new;
    int status;
    char pw_prompt[BUFSIZ];

    if (argc != 2) {
	printf("Usage: change_password loginname\n");
	return;
    }

    if (setvals(&old, argv[1]) != KADM_SUCCESS)
	return;

    new = old;

    SET_FIELD(KADM_DESKEY,new.fields);

    if (princ_exists(old.name, old.instance, krbrlm) != PE_NO) {
	/* get the admin's password */
        if (get_admin_password() != GOOD_PW)
	    return;

	/* get the new password */
	snprintf(pw_prompt, sizeof(pw_prompt), "New password for %s:", argv[1]);
	
	if (get_password(&new.key_low, &new.key_high,
			 pw_prompt, SWAP) == GOOD_PW) {
	    status = kadm_mod(&old, &new);
	    if (status == KADM_SUCCESS) {
		printf("Password changed for %s.\n", argv[1]);
	    } else {
		printf("kadmin: %s\nwhile changing password for %s",
		       error_message(status), argv[1]);
	    }
	} else
	    printf("Error reading password; password unchanged\n");
	memset(&new, 0, sizeof(new));
	if (!multiple)
	    clean_up();
    }
    else 
	printf("kadmin: Principal %s does not exist.\n",
	       krb_unparse_name_long (old.name, old.instance, krbrlm));
    return;
}

static int
getkey(unsigned char *k)
{
    int i, c;
    for (i = 0; i < 8; i++)
	{
	    c = getchar();
	    if (c == EOF)
		return 0;
	    else if (c == '\\')
		{
		    int oct = -1;
		    scanf("%03o", &oct);
		    if (oct < 0 || oct > 255)
			return 0;
		    k[i] = oct;
		}
	    else if (!isalpha(c))
		return 0;
	    else
		k[i] = c;
	}
    c = getchar();
    if (c != '\n')
	return 0;
    return 1;			/* Success */
}

static void
printkey(unsigned char *tkey)
{
    int j;
    for(j = 0; j < 8; j++)
	if(tkey[j] != '\\' && isalpha(tkey[j]) != 0)
	    printf("%c", tkey[j]);
	else
	    printf("\\%03o",(unsigned char)tkey[j]);
    printf("\n");
}

static void 
change_key(int argc, char **argv)
{
    Kadm_vals old, new;
    unsigned char newkey[8];
    int status;

    if (argc != 2) {
	printf("Usage: change_key principal-name\n");
	return;
    }

    if (setvals(&old, argv[1]) != KADM_SUCCESS)
	return;

    new = old;

    SET_FIELD(KADM_DESKEY,new.fields);

    if (princ_exists(old.name, old.instance, krbrlm) != PE_NO) {
	/* get the admin's password */
        if (get_admin_password() != GOOD_PW)
	    return;

	/* get the new password */
	printf("New DES key for %s: ", argv[1]);
	
	if (getkey(newkey)) {
	    memcpy(&new.key_low, newkey, 4);
	    memcpy(&new.key_high, ((char *)newkey) + 4, 4);
	    printf("Entered key for %s: ", argv[1]);
	    printkey(newkey);
	    memset(newkey, 0, sizeof(newkey));

	    status = kadm_mod(&old, &new);
	    if (status == KADM_SUCCESS) {
		printf("Key changed for %s.\n", argv[1]);
	    } else {
		printf("kadmin: %s\nwhile changing key for %s",
		       error_message(status), argv[1]);
	    }
	} else
	    printf("Error reading key; key unchanged\n");
	memset(&new, 0, sizeof(new));
	if (!multiple)
	    clean_up();
    }
    else 
	printf("kadmin: Principal %s does not exist.\n",
	       krb_unparse_name_long (old.name, old.instance, krbrlm));
    return;
}

static void 
change_admin_password(int argc, char **argv)
{
    des_cblock newkey;
    int status;
    char pword[MAX_KPW_LEN];
    char *pw_msg;

    if (argc != 1) {
	printf("Usage: change_admin_password\n");
	return;
    }
    if (get_pw_new_pwd(pword, sizeof(pword), &pr, 1) == 0) {
	 des_string_to_key(pword, &newkey);
	 status = kadm_change_pw_plain(newkey, pword, &pw_msg);
	 if(status == KADM_INSECURE_PW)
	      printf("Insecure password: %s\n", pw_msg);
	 else if (status == KADM_SUCCESS)
	      printf("Admin password changed\n");
	 else
	      printf("kadm error: %s\n",error_message(status));
	 memset(newkey, 0, sizeof(newkey));
	 memset(pword, 0, sizeof(pword));
    }
    if (!multiple)
	clean_up();
    return;
}

static void 
add_new_key(int argc, char **argv)
{
    Kadm_vals new;
    char pw_prompt[BUFSIZ];
    int status;

    if (argc != 2) {
	printf("Usage: add_new_key user_name.\n");
	return;
    }
    if (setvals(&new, argv[1]) != KADM_SUCCESS)
	return;

    SET_FIELD(KADM_EXPDATE,new.fields);
    SET_FIELD(KADM_ATTR,new.fields);
    SET_FIELD(KADM_MAXLIFE,new.fields);
    SET_FIELD(KADM_DESKEY,new.fields);

    if (princ_exists(new.name, new.instance, krbrlm) != PE_YES) {
	Kadm_vals vals;
	u_char fields[4];
	char n[ANAME_SZ + INST_SZ + 1];

	/* get the admin's password */
	if (get_admin_password() != GOOD_PW)
	    return;
	
	memset(fields, 0, sizeof(fields));
	SET_FIELD(KADM_NAME,fields);
	SET_FIELD(KADM_INST,fields);
	SET_FIELD(KADM_EXPDATE,fields);
	SET_FIELD(KADM_ATTR,fields);
	SET_FIELD(KADM_MAXLIFE,fields);
	snprintf (n, sizeof(n), "default.%s", new.instance);
	if (setvals(&vals, n) != KADM_SUCCESS)
	    return;

	if (kadm_get(&vals, fields) != KADM_SUCCESS) {
	    if (setvals(&vals, "default") != KADM_SUCCESS)
		return;
	    if ((status = kadm_get(&vals, fields)) != KADM_SUCCESS) {
		printf ("kadm error: %s\n", error_message(status));
		return;
	    }
	}

	if (vals.max_life == 255) /* Defaults not set! */ {
	      /* This is the default maximum lifetime for new principals. */
	      if (strcmp(new.instance, "admin") == 0)
		vals.max_life = 1 + (CLOCK_SKEW/(5*60)); /* 5+5 minutes */
	      else if (strcmp(new.instance, "root") == 0)
		vals.max_life = 96;    /* 8 hours */
	      else if (krb_life_to_time(0, 162) >= 24*60*60)
		vals.max_life = 162;     /* ca 100 hours */
	      else
		vals.max_life = 255;     /* ca 21 hours (maximum) */

	      /* Also fix expiration date. */
	      if (strcmp(new.name, "rcmd") == 0)
		vals.exp_date = 1104814999; /* Tue Jan 4 06:03:19 2005 */
	      else
		vals.exp_date = time(0) + 2*(365*24*60*60); /* + ca 2 years */
	}

	new.max_life = vals.max_life;
	new.exp_date = vals.exp_date;
	new.attributes = vals.attributes;
	get_maxlife(&new);
	get_attr(&new);
	get_expdate(&new);

	/* get the new password */
	snprintf(pw_prompt, sizeof(pw_prompt), "Password for %s:", argv[1]);
	
	if (get_password(&new.key_low, &new.key_high,
			 pw_prompt, SWAP) == GOOD_PW) {
	    status = kadm_add(&new);
	    if (status == KADM_SUCCESS) {
		printf("%s added to database.\n", argv[1]);
	    } else {
		printf("kadm error: %s\n",error_message(status));
	    }
	} else
	    printf("Error reading password; %s not added\n",argv[1]);
	memset(&new, 0, sizeof(new));
	if (!multiple)
	    clean_up();
    }
    else
	printf("kadmin: Principal already exists.\n");
    return;
}

static void 
del_entry(int argc, char **argv)
{
    int status;
    Kadm_vals vals;

    if (argc != 2) {
	printf("Usage: del_entry username\n");
	return;
    }

    if (setvals(&vals, argv[1]) != KADM_SUCCESS)
	return;

    if (princ_exists(vals.name, vals.instance, krbrlm) != PE_NO) {
	/* get the admin's password */
	if (get_admin_password() != GOOD_PW)
	    return;
	
	if ((status = kadm_del(&vals)) == KADM_SUCCESS){
	    printf("%s removed from database.\n", argv[1]);
	} else {
	    printf("kadm error: %s\n",error_message(status));
	}
	
	if (!multiple)
	    clean_up();
    }
    else
	printf("kadmin: Principal %s does not exist.\n",
	       krb_unparse_name_long (vals.name, vals.instance, krbrlm));
    return;
}

static void 
get_entry(int argc, char **argv)
{
    int status;
    u_char fields[4];
    Kadm_vals vals;

    if (argc != 2) {
	printf("Usage: get_entry username\n");
	return;
    }

    memset(fields, 0, sizeof(fields));

    SET_FIELD(KADM_NAME,fields);
    SET_FIELD(KADM_INST,fields);
    SET_FIELD(KADM_EXPDATE,fields);
    SET_FIELD(KADM_ATTR,fields);
    SET_FIELD(KADM_MAXLIFE,fields);
#if 0
    SET_FIELD(KADM_DESKEY,fields); 
#endif

    if (setvals(&vals, argv[1]) != KADM_SUCCESS)
	return;


    if (princ_exists(vals.name, vals.instance, krbrlm) != PE_NO) {
	/* get the admin's password */
	if (get_admin_password() != GOOD_PW)
	    return;
	
	if ((status = kadm_get(&vals, fields)) == KADM_SUCCESS)
	    prin_vals(&vals);
	else
	    printf("kadm error: %s\n",error_message(status));
	
	if (!multiple)
	    clean_up();
    }
    else
	printf("kadmin: Principal %s does not exist.\n",
	       krb_unparse_name_long (vals.name, vals.instance, krbrlm));
    return;
}

static void 
mod_entry(int argc, char **argv)
{
    int status;
    u_char fields[4];
    Kadm_vals ovals, nvals;

    if (argc != 2) {
	printf("Usage: mod_entry username\n");
	return;
    }

    memset(fields, 0, sizeof(fields));

    SET_FIELD(KADM_NAME,fields);
    SET_FIELD(KADM_INST,fields);
    SET_FIELD(KADM_EXPDATE,fields);
    SET_FIELD(KADM_ATTR,fields);
    SET_FIELD(KADM_MAXLIFE,fields);

    if (setvals(&ovals, argv[1]) != KADM_SUCCESS)
	return;

    nvals = ovals;

    if (princ_exists(ovals.name, ovals.instance, krbrlm) == PE_NO) {
	printf("kadmin: Principal %s does not exist.\n",
	       krb_unparse_name_long (ovals.name, ovals.instance, krbrlm));
	return;
    }

    /* get the admin's password */
    if (get_admin_password() != GOOD_PW)
	return;
	
    if ((status = kadm_get(&ovals, fields)) != KADM_SUCCESS) {
	printf("[ unable to retrieve current settings: %s ]\n",
	    error_message(status));
	nvals.max_life = DEFAULT_TKT_LIFE;
	nvals.exp_date = 0;
	nvals.attributes = 0;
    } else {
	nvals.max_life = ovals.max_life;
	nvals.exp_date = ovals.exp_date;
	nvals.attributes = ovals.attributes;
    }

    get_maxlife(&nvals);
    get_attr(&nvals);
    get_expdate(&nvals);
    
    if (IS_FIELD(KADM_MAXLIFE, nvals.fields) ||
	IS_FIELD(KADM_ATTR, nvals.fields) ||
	IS_FIELD(KADM_EXPDATE, nvals.fields)) {
	if ((status = kadm_mod(&ovals, &nvals)) != KADM_SUCCESS) {
	    printf("kadm error: %s\n",error_message(status));
	    goto out;
	}
	if ((status = kadm_get(&ovals, fields)) != KADM_SUCCESS) {
	    printf("kadm error: %s\n",error_message(status));
	    goto out;
	}
    }
    prin_vals(&ovals);

out:
    if (!multiple)
	clean_up();
    return;
}

static void
help(int argc, char **argv)
{
    sl_help (cmds, argc, argv);
}
