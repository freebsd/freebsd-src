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
/* $FreeBSD$ */

/*
 * Kerberos database administrator's tool.  
 * 
 * The default behavior of kadmin is if the -m option is given 
 * on the commandline, multiple requests are allowed to be given
 * with one entry of the admin password (until the tickets expire).
 */

#include "kadm_locl.h"
#include "getarg.h"
#include "parse_time.h"

RCSID("$Id: kadmin.c,v 1.62 1999/11/02 17:02:14 bg Exp $");

static int change_password(int argc, char **argv);
static int change_key(int argc, char **argv);
static int change_admin_password(int argc, char **argv);
static int add_new_key(int argc, char **argv);
static int del_entry(int argc, char **argv);
static int get_entry(int argc, char **argv);
static int mod_entry(int argc, char **argv);
static int help(int argc, char **argv);
static int clean_up_cmd(int argc, char **argv);
static int quit_cmd(int argc, char **argv);
static int set_timeout_cmd(int argc, char **argv);

static int set_timeout(const char *);

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
  {"set_timeout", set_timeout_cmd, "Set ticket timeout"},
  {"timeout" },
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

static time_t
parse_expdate(const char *str)
{
    struct tm edate;

    memset(&edate, 0, sizeof(edate));
    if (sscanf(str, "%d-%d-%d",
	       &edate.tm_year, &edate.tm_mon, &edate.tm_mday) == 3) {
	edate.tm_mon--;     /* January is 0, not 1 */
	edate.tm_hour = 23; /* nearly midnight at the end of the */
	edate.tm_min = 59;  /* specified day */
    }
    if(krb_check_tm (edate))
	return -1;
    edate.tm_year -= 1900;
    return tm2time (edate, 1);
}

static void
get_expdate(Kadm_vals *vals)
{
    char buff[BUFSIZ];
    time_t t;

    do {
	strftime(buff, sizeof(buff), "%Y-%m-%d", k_localtime(&vals->exp_date));
        printf("Expiration date (enter yyyy-mm-dd) ?  [%s]  ", buff);
        fflush(stdout);
        if (fgets(buff, sizeof(buff), stdin) == NULL || *buff == '\n') {
            clearerr(stdin);
            return;
        }
	t = parse_expdate(buff);
    }while(t < 0);
    vals->exp_date = t;
    SET_FIELD(KADM_EXPDATE,vals->fields);
}

static int
princ_exists(char *name, char *instance, char *realm)
{
    int status;

    int old = krb_use_admin_server(1);
    status = krb_get_pw_in_tkt(name, instance, realm,
			       KRB_TICKET_GRANTING_TICKET,
			       realm, 1, "");
    krb_use_admin_server(old);

    if ((status == KSUCCESS) || (status == INTK_BADPW))
	return(PE_YES);
    else if (status == KDC_PR_UNKNOWN)
	return(PE_NO);
    else
	return(PE_UNSURE);
}

static void
passwd_to_lowhigh(u_int32_t *low, u_int32_t *high, char *password, int byteswap)
{
    des_cblock newkey;

    if (strlen(password) == 0) {
    	printf("Using random password.\n");
#ifdef NOENCRYPTION
	memset(newkey, 0, sizeof(newkey));
#else
	des_random_key(newkey);
#endif
    } else {
#ifdef NOENCRYPTION
      memset(newkey, 0, sizeof(newkey));
#else
      des_string_to_key(password, &newkey);
#endif
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
}

static int
get_password(u_int32_t *low, u_int32_t *high, char *prompt, int byteswap)
{
    char new_passwd[MAX_KPW_LEN];	/* new password */

    if (read_long_pw_string(new_passwd, sizeof(new_passwd)-1, prompt, 1))
    	return(BAD_PW);
    passwd_to_lowhigh (low, high, new_passwd, byteswap);
    memset (new_passwd, 0, sizeof(new_passwd));
    return(GOOD_PW);
}

static int
get_admin_password(void)
{
    int status;
    char admin_passwd[MAX_KPW_LEN];	/* Admin's password */
    int ticket_life = 1;	/* minimum ticket lifetime */
    CREDENTIALS c;

    alarm(0);
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
    
    if (princ_exists(pr.name, pr.instance, pr.realm) != PE_NO) {
        char prompt[256];
	snprintf(prompt, sizeof(prompt), "%s's Password: ", 
		 krb_unparse_name(&pr));
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

static char *principal;
static char *username;
static char *realm;
static char *timeout;
static int tflag; /* use existing tickets */
static int mflag; /* compatibility */
static int version_flag;
static int help_flag;

static time_t destroy_timeout = 5 * 60;

struct getargs args[] = {
    { NULL,	'p',	arg_string, &principal, 
      "principal to authenticate as"},
    { NULL,	'u',	arg_string, &username, 
      "username, other than default" },
    { NULL,	'r',	arg_string, &realm, "local realm" },
    { NULL,	'm',	arg_flag, &mflag, "disable ticket timeout" },
    { NULL,	'T',	arg_string, &timeout, "default ticket timeout" },
    { NULL,	't',	arg_flag, &tflag, "use existing tickets" },
    { "version",0,	arg_flag, &version_flag },
    { "help",	'h',	arg_flag, &help_flag },
};

static int num_args = sizeof(args) / sizeof(args[0]);

static int
clean_up()
{
    if(!tflag)
	return dest_tkt() == KSUCCESS;
    return 0; 
}

static int
clean_up_cmd (int argc, char **argv)
{
    clean_up();
    return 0;
}

static int
quit_cmd (int argc, char **argv)
{
    return 1;
}

static void 
usage(int code)
{
    arg_printusage(args, num_args, NULL, "[command]");
    exit(code);
}

static int
do_init(int argc, char **argv)
{
    int optind = 0;
    int ret;

    set_progname (argv[0]);
    
    if(getarg(args, num_args, argc, argv, &optind) < 0)
	usage(1);
    if(help_flag)
	usage(0);
    if(version_flag) {
	print_version(NULL);
	exit(0);
    }

    memset(&pr, 0, sizeof(pr));
    ret = krb_get_default_principal(pr.name, pr.instance, default_realm);
    if(ret < 0)
	errx(1, "Can't figure out default principal");
    if(pr.instance[0] == '\0')
	strlcpy(pr.instance, "admin", sizeof(pr.instance));
    if(principal) {
	if(username)
	    warnx("Ignoring username when principal is given");
	ret = krb_parse_name(principal, &pr);
	if(ret)
	    errx(1, "%s: %s", principal, krb_get_err_text(ret));
	if(pr.realm[0] != '\0')
	    strlcpy(default_realm, pr.realm, sizeof(default_realm));
    } else if(username) {
	strlcpy(pr.name, username, sizeof(pr.name));
	strlcpy(pr.instance, "admin", sizeof(pr.instance));
    } 
    
    if(realm)
	strlcpy(default_realm, realm, sizeof(default_realm));
    
    strlcpy(krbrlm, default_realm, sizeof(krbrlm));

    if(pr.realm[0] == '\0')
	strlcpy(pr.realm, krbrlm, sizeof(pr.realm));

    if (kadm_init_link(PWSERV_NAME, KRB_MASTER, krbrlm) != KADM_SUCCESS)
	*krbrlm = '\0';
    
    if(timeout) {
	if(set_timeout(timeout) == -1)
	    warnx("bad timespecification `%s'", timeout);
    } else if(mflag)
	destroy_timeout = 0;

    if (tflag)
	destroy_timeout = 0; /* disable timeout */
    else{
	char tktstring[128];
	snprintf(tktstring, sizeof(tktstring), "%s_adm_%d",
		 TKT_ROOT, (int)getpid());
	krb_set_tkt_string(tktstring);
    }
    return optind;
}

static void
sigalrm(int sig)
{
    if(clean_up())
	printf("\nTickets destroyed.\n");
}

int
main(int argc, char **argv)
{
    int optind = do_init(argc, argv);
    if(argc > optind)
	sl_command(cmds, argc - optind, argv + optind);
    else {
	void *data = NULL;
	signal(SIGALRM, sigalrm);
	while(sl_command_loop(cmds, "kadmin: ", &data) == 0)
	    alarm(destroy_timeout);
    }
    clean_up();
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
	strlcpy(realm, default_realm, sizeof(realm));
    if (strcmp(realm, krbrlm)) {
	strlcpy(krbrlm, realm, sizeof(krbrlm));
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

static int 
set_timeout(const char *timespec)
{
    int t = parse_time(timespec, "s");
    if(t == -1)
	return -1;
    destroy_timeout = t;
    return 0;
}

static int
set_timeout_cmd(int argc, char **argv)
{
    char ts[128];
    if (argc > 2) {
	printf("Usage: set_timeout [timeout]\n");
	return 0;
    }
    if(argc == 2) {
	if(set_timeout(argv[1]) == -1){
	    printf("Bad time specification `%s'\n", argv[1]);
	    return 0;
	}
    }
    if(destroy_timeout == 0)
	printf("Timeout disabled.\n");
    else{
	unparse_time(destroy_timeout, ts, sizeof(ts));
	printf("Timeout after %s.\n", ts);
    }
    return 0;
}

static int 
change_password(int argc, char **argv)
{
    Kadm_vals old, new;
    int status;
    char pw_prompt[BUFSIZ];

    char pw[32];
    int generate_password = 0;
    int i;
    int optind = 0;
    char *user = NULL;

    struct getargs cpw_args[] = {
	{ "random", 'r', arg_flag, NULL, "generate random password" },
    };
    i = 0;
    cpw_args[i++].value = &generate_password;

    if(getarg(cpw_args, sizeof(cpw_args) / sizeof(cpw_args[0]), 
	      argc, argv, &optind)){
	arg_printusage(cpw_args, 
		       sizeof(cpw_args) / sizeof(cpw_args[0]), 
		       "cpw",
		       "principal");
	return 0;
    }

    argc -= optind;
    argv += optind;

    if (argc != 1) {
	printf("Usage: change_password [options] principal\n");
	return 0;
    }

    user = argv[0];

    if (setvals(&old, user) != KADM_SUCCESS)
	return 0;

    new = old;

    SET_FIELD(KADM_DESKEY,new.fields);

    if (princ_exists(old.name, old.instance, krbrlm) != PE_NO) {
	/* get the admin's password */
        if (get_admin_password() != GOOD_PW)
	    return 0;


	if (generate_password) {
	    random_password(pw, sizeof(pw), &new.key_low, &new.key_high);
	} else {
	    /* get the new password */
	    snprintf(pw_prompt, sizeof(pw_prompt), 
		     "New password for %s:", user);
	
	    if (get_password(&new.key_low, &new.key_high,
			     pw_prompt, SWAP) != GOOD_PW) {
		printf("Error reading password; password unchanged\n");
		return 0;
	    }
	}

	status = kadm_mod(&old, &new);
	if (status == KADM_SUCCESS) {
	    printf("Password changed for %s.\n", user);
	    if (generate_password)
		printf("Password is: %s\n", pw);
	} else {
	    printf("kadmin: %s\nwhile changing password for %s",
		   error_message(status), user);
	}

	memset(pw, 0, sizeof(pw));
	memset(&new, 0, sizeof(new));
    } else 
	printf("kadmin: Principal %s does not exist.\n",
	       krb_unparse_name_long (old.name, old.instance, krbrlm));
    return 0;
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

static int 
change_key(int argc, char **argv)
{
    Kadm_vals old, new;
    unsigned char newkey[8];
    int status;

    if (argc != 2) {
	printf("Usage: change_key principal-name\n");
	return 0;
    }

    if (setvals(&old, argv[1]) != KADM_SUCCESS)
	return 0;

    new = old;

    SET_FIELD(KADM_DESKEY,new.fields);

    if (princ_exists(old.name, old.instance, krbrlm) != PE_NO) {
	/* get the admin's password */
        if (get_admin_password() != GOOD_PW)
	    return 0;

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
    }
    else 
	printf("kadmin: Principal %s does not exist.\n",
	       krb_unparse_name_long (old.name, old.instance, krbrlm));
    return 0;
}

static int 
change_admin_password(int argc, char **argv)
{
    des_cblock newkey;
    int status;
    char pword[MAX_KPW_LEN];
    char *pw_msg;

    alarm(0);
    if (argc != 1) {
	printf("Usage: change_admin_password\n");
	return 0;
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
    return 0;
}

void random_password(char*, size_t, u_int32_t*, u_int32_t*);

static int 
add_new_key(int argc, char **argv)
{
    int i;
    char pw_prompt[BUFSIZ];
    int status;
    int generate_password = 0;
    char *password = NULL;

    char *expiration_string = NULL;
    time_t default_expiration = 0;
    int expiration_set = 0;

    char *life_string = NULL;
    time_t default_life = 0;
    int life_set = 0;

    int attributes = -1;
    int default_attributes = 0;
    int attributes_set = 0;

    int optind = 0;

    /* XXX remember to update value assignments below */
    struct getargs add_args[] = {
	{ "random", 'r', arg_flag, NULL, "generate random password" },
	{ "password", 'p', arg_string, NULL },
	{ "life", 'l', arg_string, NULL, "max ticket life" },
	{ "expiration", 'e', arg_string, NULL, "principal expiration" },
	{ "attributes", 'a', arg_integer, NULL }
    };
    i = 0;
    add_args[i++].value = &generate_password;
    add_args[i++].value = &password;
    add_args[i++].value = &life_string;
    add_args[i++].value = &expiration_string;
    add_args[i++].value = &attributes;


    if(getarg(add_args, sizeof(add_args) / sizeof(add_args[0]), 
	      argc, argv, &optind)){
	arg_printusage(add_args, 
		       sizeof(add_args) / sizeof(add_args[0]), 
		       "add",
		       "principal ...");
	return 0;
    }

    if(expiration_string) {
	default_expiration = parse_expdate(expiration_string);
	if(default_expiration < 0)
	    warnx("Unknown expiration date `%s'", expiration_string);
	else
	    expiration_set = 1;
    }
    if(life_string) {
	time_t t = parse_time(life_string, "hour");
	if(t == -1) 
	    warnx("Unknown lifetime `%s'", life_string);
	else {
	    default_life = krb_time_to_life(0, t);
	    life_set = 1;
	}
    }
    if(attributes != -1) {
	default_attributes = attributes;
	attributes_set = 1;
    }


    {
	char default_name[ANAME_SZ + INST_SZ + 1];
	char old_default[INST_SZ + 1] = "";
	Kadm_vals new, default_vals;
	char pw[32];
	u_char fields[4];

	for(i = optind; i < argc; i++) {
	    if (setvals(&new, argv[i]) != KADM_SUCCESS)
		return 0;
	    SET_FIELD(KADM_EXPDATE, new.fields);
	    SET_FIELD(KADM_ATTR, new.fields);
	    SET_FIELD(KADM_MAXLIFE, new.fields);
	    SET_FIELD(KADM_DESKEY, new.fields);

	    if (princ_exists(new.name, new.instance, krbrlm) == PE_YES) {
		printf("kadmin: Principal %s already exists.\n", argv[i]);
		continue;
	    }
	    /* get the admin's password */
	    if (get_admin_password() != GOOD_PW)
		return 0;
	
	    snprintf (default_name, sizeof(default_name), 
		      "default.%s", new.instance);
	    if(strcmp(old_default, default_name) != 0) {
		memset(fields, 0, sizeof(fields));
		SET_FIELD(KADM_NAME, fields);
		SET_FIELD(KADM_INST, fields);
		SET_FIELD(KADM_EXPDATE, fields);
		SET_FIELD(KADM_ATTR, fields);
		SET_FIELD(KADM_MAXLIFE, fields);
		if (setvals(&default_vals, default_name) != KADM_SUCCESS)
		    return 0;
	
		if (kadm_get(&default_vals, fields) != KADM_SUCCESS) {
		    /* no such entry, try just `default' */
		    if (setvals(&default_vals, "default") != KADM_SUCCESS)
			continue;
		    if ((status = kadm_get(&default_vals, fields)) != KADM_SUCCESS) {
			warnx ("kadm error: %s", error_message(status));
			break; /* no point in continuing */
		    }
		}

		if (default_vals.max_life == 255) /* Defaults not set! */ {
		    /* This is the default maximum lifetime for new principals. */
		    if (strcmp(new.instance, "admin") == 0)
			default_vals.max_life = 1 + (CLOCK_SKEW/(5*60)); /* 5+5 minutes */
		    else if (strcmp(new.instance, "root") == 0)
			default_vals.max_life = 96;    /* 8 hours */
		    else if (krb_life_to_time(0, 162) >= 24*60*60)
			default_vals.max_life = 162;     /* ca 100 hours */
		    else
			default_vals.max_life = 255;     /* ca 21 hours (maximum) */
		
		    /* Also fix expiration date. */
		    {
			time_t now;
			struct tm tm;

			now = time(0);
			tm = *gmtime(&now);
			if (strcmp(new.name, "rcmd") == 0 ||
			    strcmp(new.name, "ftp")  == 0 ||
			    strcmp(new.name, "pop")  == 0)
			    tm.tm_year += 5;
			else
			    tm.tm_year += 2;
			default_vals.exp_date = mktime(&tm);
		    }		
		    default_vals.attributes = default_vals.attributes;
		}
		if(!life_set)
		    default_life = default_vals.max_life;
		if(!expiration_set)
		    default_expiration = default_vals.exp_date;
		if(!attributes_set)
		    default_attributes = default_vals.attributes;
	    }

	    new.max_life = default_life;
	    new.exp_date = default_expiration;
	    new.attributes = default_attributes;
	    if(!life_set)
		get_maxlife(&new);
	    if(!attributes_set)
		get_attr(&new);
	    if(!expiration_set)
		get_expdate(&new);

	    if(generate_password) {
		random_password(pw, sizeof(pw), &new.key_low, &new.key_high);
	    } else if (password == NULL) {
		/* get the new password */
		snprintf(pw_prompt, sizeof(pw_prompt), "Password for %s:", 
			 argv[i]);
	
		if (get_password(&new.key_low, &new.key_high,
				 pw_prompt, SWAP) != GOOD_PW) {
		    printf("Error reading password: %s not added\n", argv[i]);
		    memset(&new, 0, sizeof(new));
		    return 0;
		}
	    } else {
		passwd_to_lowhigh (&new.key_low, &new.key_high, password, SWAP);
		memset (password, 0, strlen(password));
	    }

	    status = kadm_add(&new);
	    if (status == KADM_SUCCESS) {
		printf("%s added to database", argv[i]);
		if (generate_password)
		    printf (" with password `%s'", pw);
		printf (".\n");
	    } else 
		printf("kadm error: %s\n",error_message(status));
		
	    memset(pw, 0, sizeof(pw));
	    memset(&new, 0, sizeof(new));
	}
    }
    
    return 0;
}

static int 
del_entry(int argc, char **argv)
{
    int status;
    Kadm_vals vals;
    int i;

    if (argc < 2) {
	printf("Usage: delete principal...\n");
	return 0;
    }

    for(i = 1; i < argc; i++) {
	if (setvals(&vals, argv[i]) != KADM_SUCCESS)
	    return 0;
	
	if (princ_exists(vals.name, vals.instance, krbrlm) != PE_NO) {
	    /* get the admin's password */
	    if (get_admin_password() != GOOD_PW)
		return 0;
	    
	    if ((status = kadm_del(&vals)) == KADM_SUCCESS)
		printf("%s removed from database.\n", argv[i]);
	    else 
		printf("kadm error: %s\n",error_message(status));
	}
	else
	    printf("kadmin: Principal %s does not exist.\n",
		   krb_unparse_name_long (vals.name, vals.instance, krbrlm));
    }
    return 0;
}

static int 
get_entry(int argc, char **argv)
{
    int status;
    u_char fields[4];
    Kadm_vals vals;

    if (argc != 2) {
	printf("Usage: get_entry username\n");
	return 0;
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
#ifdef EXTENDED_KADM
    SET_FIELD(KADM_MODDATE, fields);
    SET_FIELD(KADM_MODNAME, fields);
    SET_FIELD(KADM_MODINST, fields);
    SET_FIELD(KADM_KVNO, fields);
#endif

    if (setvals(&vals, argv[1]) != KADM_SUCCESS)
	return 0;


    if (princ_exists(vals.name, vals.instance, krbrlm) != PE_NO) {
	/* get the admin's password */
	if (get_admin_password() != GOOD_PW)
	    return 0;
	
	if ((status = kadm_get(&vals, fields)) == KADM_SUCCESS)
	    prin_vals(&vals);
	else
	    printf("kadm error: %s\n",error_message(status));
    }
    else
	printf("kadmin: Principal %s does not exist.\n",
	       krb_unparse_name_long (vals.name, vals.instance, krbrlm));
    return 0;
}

static int 
mod_entry(int argc, char **argv)
{
    int status;
    u_char fields[4];
    Kadm_vals ovals, nvals;
    int i;

    char *expiration_string = NULL;
    time_t default_expiration = 0;
    int expiration_set = 0;

    char *life_string = NULL;
    time_t default_life = 0;
    int life_set = 0;

    int attributes = -1;
    int default_attributes = 0;
    int attributes_set = 0;

    int optind = 0;

    /* XXX remember to update value assignments below */
    struct getargs mod_args[] = {
	{ "life", 'l', arg_string, NULL, "max ticket life" },
	{ "expiration", 'e', arg_string, NULL, "principal expiration" },
	{ "attributes", 'a', arg_integer, NULL }
    };
    i = 0;
    mod_args[i++].value = &life_string;
    mod_args[i++].value = &expiration_string;
    mod_args[i++].value = &attributes;


    if(getarg(mod_args, sizeof(mod_args) / sizeof(mod_args[0]), 
	      argc, argv, &optind)){
	arg_printusage(mod_args, 
		       sizeof(mod_args) / sizeof(mod_args[0]), 
		       "mod",
		       "principal ...");
	return 0;
    }

    if(expiration_string) {
	default_expiration = parse_expdate(expiration_string);
	if(default_expiration < 0)
	    warnx("Unknown expiration date `%s'", expiration_string);
	else
	    expiration_set = 1;
    }
    if(life_string) {
	time_t t = parse_time(life_string, "hour");
	if(t == -1) 
	    warnx("Unknown lifetime `%s'", life_string);
	else {
	    default_life = krb_time_to_life(0, t);
	    life_set = 1;
	}
    }
    if(attributes != -1) {
	default_attributes = attributes;
	attributes_set = 1;
    }


    for(i = optind; i < argc; i++) {

	memset(fields, 0, sizeof(fields));

	SET_FIELD(KADM_NAME,fields);
	SET_FIELD(KADM_INST,fields);
	SET_FIELD(KADM_EXPDATE,fields);
	SET_FIELD(KADM_ATTR,fields);
	SET_FIELD(KADM_MAXLIFE,fields);

	if (setvals(&ovals, argv[i]) != KADM_SUCCESS)
	    return 0;

	nvals = ovals;

	if (princ_exists(ovals.name, ovals.instance, krbrlm) == PE_NO) {
	    printf("kadmin: Principal %s does not exist.\n",
		   krb_unparse_name_long (ovals.name, ovals.instance, krbrlm));
	    return 0;
	}

	/* get the admin's password */
	if (get_admin_password() != GOOD_PW)
	    return 0;
	
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

	if(life_set) {
	    nvals.max_life = default_life;
	    SET_FIELD(KADM_MAXLIFE, nvals.fields);
	} else
	    get_maxlife(&nvals);
	if(attributes_set) {
	    nvals.attributes = default_attributes;
	    SET_FIELD(KADM_ATTR, nvals.fields);
	} else
	    get_attr(&nvals);
	if(expiration_set) {
	    nvals.exp_date = default_expiration;
	    SET_FIELD(KADM_EXPDATE, nvals.fields);
	} else
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
    }
    
out:
    return 0;
}

static int
help(int argc, char **argv)
{
    sl_help (cmds, argc, argv);
    return 0;
}
