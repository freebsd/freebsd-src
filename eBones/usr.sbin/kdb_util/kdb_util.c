/*
 * Copyright 1987, 1988 by the Massachusetts Institute of Technology.
 * For copying and distribution information, please see the file
 * <Copyright.MIT>.
 *
 * Kerberos database manipulation utility. This program allows you to
 * dump a kerberos database to an ascii readable file and load this
 * file into the database. Read locking of the database is done during a
 * dump operation. NO LOCKING is done during a load operation. Loads
 * should happen with other processes shutdown.
 *
 * Written July 9, 1987 by Jeffrey I. Schiller
 *
 *	from: kdb_util.c,v 4.4 90/01/09 15:57:20 raeburn Exp $
 *	$FreeBSD$
 */

#if 0
#ifndef	lint
static char rcsid[] =
"$FreeBSD$";
#endif	lint
#endif

#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <time.h>
#include <strings.h>
#include <des.h>
#include <krb.h>
#include <sys/file.h>
#include <krb_db.h>

#define TRUE 1

Principal aprinc;

static des_cblock master_key, new_master_key;
static des_key_schedule master_key_schedule, new_master_key_schedule;

#define zaptime(foo) bzero((char *)(foo), sizeof(*(foo)))

char * progname;

void convert_old_format_db (char *db_file, FILE *out);
void convert_new_master_key (char *db_file, FILE *out);
void update_ok_file (char *file_name);
void print_time(FILE *file, unsigned long timeval);
void load_db (char *db_file, FILE *input_file);
int dump_db (char *db_file, FILE *output_file, void (*cv_key)());

int
main(argc, argv)
    int     argc;
    char  **argv;
{
    FILE   *file;
    enum {
	OP_LOAD,
	OP_DUMP,
	OP_SLAVE_DUMP,
	OP_NEW_MASTER,
	OP_CONVERT_OLD_DB,
    }       op;
    char *file_name;
    char *prog = argv[0];
    char *db_name;

    progname = prog;

    if (argc != 3 && argc != 4) {
	fprintf(stderr, "Usage: %s operation file-name [database name].\n",
		argv[0]);
	exit(1);
    }
    if (argc == 3)
	db_name = DBM_FILE;
    else
	db_name = argv[3];

    if (kerb_db_set_name (db_name) != 0) {
	perror("Can't open database");
	exit(1);
    }

    if (!strcmp(argv[1], "load"))
	op = OP_LOAD;
    else if (!strcmp(argv[1], "dump"))
	op = OP_DUMP;
    else if (!strcmp(argv[1], "slave_dump"))
        op = OP_SLAVE_DUMP;
    else if (!strcmp(argv[1], "new_master_key"))
        op = OP_NEW_MASTER;
    else if (!strcmp(argv[1], "convert_old_db"))
        op = OP_CONVERT_OLD_DB;
    else {
	fprintf(stderr,
	    "%s: %s is an invalid operation.\n", prog, argv[1]);
	fprintf(stderr,
	    "%s: Valid operations are \"dump\", \"slave_dump\",", argv[0]);
	fprintf(stderr,
		"\"load\", \"new_master_key\", and \"convert_old_db\".\n");
	exit(1);
    }

    file_name = argv[2];
    file = fopen(file_name, op == OP_LOAD ? "r" : "w");
    if (file == NULL) {
	fprintf(stderr, "%s: Unable to open %s\n", prog, argv[2]);
	(void) fflush(stderr);
	perror("open");
	exit(1);
    }

    switch (op) {
    case OP_DUMP:
      if ((dump_db (db_name, file, (void (*)()) 0) == EOF) ||
	  (fclose(file) == EOF)) {
	  fprintf(stderr, "error on file %s:", file_name);
	  perror("");
	  exit(1);
      }
      break;
    case OP_SLAVE_DUMP:
      if ((dump_db (db_name, file, (void (*)()) 0) == EOF) ||
	  (fclose(file) == EOF)) {
	  fprintf(stderr, "error on file %s:", file_name);
	  perror("");
	  exit(1);
      }
      update_ok_file (file_name);
      break;
    case OP_LOAD:
      load_db (db_name, file);
      break;
    case OP_NEW_MASTER:
      convert_new_master_key (db_name, file);
      printf("Don't forget to do a `kdb_util load %s' to reload the database!\n", file_name);
      break;
    case OP_CONVERT_OLD_DB:
      convert_old_format_db (db_name, file);
      printf("Don't forget to do a `kdb_util load %s' to reload the database!\n", file_name);
      break;
    }
    exit(0);
  }

void
clear_secrets ()
{
  bzero((char *)master_key, sizeof (des_cblock));
  bzero((char *)master_key_schedule, sizeof (Key_schedule));
  bzero((char *)new_master_key, sizeof (des_cblock));
  bzero((char *)new_master_key_schedule, sizeof (Key_schedule));
}

/* cv_key is a procedure which takes a principle and changes its key,
   either for a new method of encrypting the keys, or a new master key.
   if cv_key is null no transformation of key is done (other than net byte
   order). */

struct callback_args {
    void (*cv_key)();
    FILE *output_file;
};

static int dump_db_1(arg, principal)
    char *arg;
    Principal *principal;
{	    /* replace null strings with "*" */
    struct callback_args *a = (struct callback_args *)arg;

    if (principal->instance[0] == '\0') {
	principal->instance[0] = '*';
	principal->instance[1] = '\0';
    }
    if (principal->mod_name[0] == '\0') {
	principal->mod_name[0] = '*';
	principal->mod_name[1] = '\0';
    }
    if (principal->mod_instance[0] == '\0') {
	principal->mod_instance[0] = '*';
	principal->mod_instance[1] = '\0';
    }
    if (a->cv_key != NULL) {
	(*a->cv_key) (principal);
    }
    fprintf(a->output_file, "%s %s %d %d %d %d %lx %lx",
	    principal->name,
	    principal->instance,
	    principal->max_life,
	    principal->kdc_key_ver,
	    principal->key_version,
	    principal->attributes,
	    htonl (principal->key_low),
	    htonl (principal->key_high));
    print_time(a->output_file, principal->exp_date);
    print_time(a->output_file, principal->mod_date);
    fprintf(a->output_file, " %s %s\n",
	    principal->mod_name,
	    principal->mod_instance);
    return 0;
}

int
dump_db (db_file, output_file, cv_key)
     char *db_file;
     FILE *output_file;
     void (*cv_key)();
{
    struct callback_args a;

    a.cv_key = cv_key;
    a.output_file = output_file;

    kerb_db_iterate (dump_db_1, (char *)&a);
    return fflush(output_file);
}

void
load_db (db_file, input_file)
     char *db_file;
     FILE *input_file;
{
    char    exp_date_str[50];
    char    mod_date_str[50];
    int     temp1, temp2, temp3;
    long time_explode();
    int code;
    char *temp_db_file;
    temp1 = strlen(db_file)+2;
    temp_db_file = malloc (temp1);
    strcpy(temp_db_file, db_file);
    strcat(temp_db_file, "~");

    /* Create the database */
    if ((code = kerb_db_create(temp_db_file)) != 0) {
	fprintf(stderr, "Couldn't create temp database %s: %s\n",
		temp_db_file, sys_errlist[code]);
	exit(1);
    }
    kerb_db_set_name(temp_db_file);
    for (;;) {			/* explicit break on eof from fscanf */
	bzero((char *)&aprinc, sizeof(aprinc));
	if (fscanf(input_file,
		   "%s %s %d %d %d %hd %lx %lx %s %s %s %s\n",
		   aprinc.name,
		   aprinc.instance,
		   &temp1,
		   &temp2,
		   &temp3,
		   &aprinc.attributes,
		   &aprinc.key_low,
		   &aprinc.key_high,
		   exp_date_str,
		   mod_date_str,
		   aprinc.mod_name,
		   aprinc.mod_instance) == EOF)
	    break;
	aprinc.key_low = ntohl (aprinc.key_low);
	aprinc.key_high = ntohl (aprinc.key_high);
	aprinc.max_life = (unsigned char) temp1;
	aprinc.kdc_key_ver = (unsigned char) temp2;
	aprinc.key_version = (unsigned char) temp3;
	aprinc.exp_date = time_explode(exp_date_str);
	aprinc.mod_date = time_explode(mod_date_str);
	if (aprinc.instance[0] == '*')
	    aprinc.instance[0] = '\0';
	if (aprinc.mod_name[0] == '*')
	    aprinc.mod_name[0] = '\0';
	if (aprinc.mod_instance[0] == '*')
	    aprinc.mod_instance[0] = '\0';
	if (kerb_db_put_principal(&aprinc, 1) != 1) {
	    fprintf(stderr, "Couldn't store %s.%s: %s; load aborted\n",
		    aprinc.name, aprinc.instance,
		    sys_errlist[errno]);
	    exit(1);
	};
    }
    if ((code = kerb_db_rename(temp_db_file, db_file)) != 0)
	perror("database rename failed");
    (void) fclose(input_file);
    free(temp_db_file);
}

void
print_time(file, timeval)
    FILE   *file;
    unsigned long timeval;
{
    struct tm *tm;
    struct tm *gmtime();
    tm = gmtime((long *)&timeval);
    fprintf(file, " %04d%02d%02d%02d%02d",
            tm->tm_year < 1900 ? tm->tm_year + 1900: tm->tm_year,
            tm->tm_mon + 1,
            tm->tm_mday,
            tm->tm_hour,
            tm->tm_min);
}

/*ARGSUSED*/
void
update_ok_file (file_name)
     char *file_name;
{
    /* handle slave locking/failure stuff */
    char *file_ok;
    int fd;
    static char ok[]=".dump_ok";

    if ((file_ok = (char *)malloc(strlen(file_name) + strlen(ok) + 1))
	== NULL) {
	fprintf(stderr, "kdb_util: out of memory.\n");
	(void) fflush (stderr);
	perror ("malloc");
	exit (1);
    }
    strcpy(file_ok, file_name);
    strcat(file_ok, ok);
    if ((fd = open(file_ok, O_WRONLY|O_CREAT|O_TRUNC, 0400)) < 0) {
	fprintf(stderr, "Error creating 'ok' file, '%s'", file_ok);
	perror("");
	(void) fflush (stderr);
	exit (1);
    }
    free(file_ok);
    close(fd);
}

void
convert_key_new_master (p)
     Principal *p;
{
  des_cblock key;

  /* leave null keys alone */
  if ((p->key_low == 0) && (p->key_high == 0)) return;

  /* move current key to des_cblock for encryption, special case master key
     since that's changing */
  if ((strncmp (p->name, KERB_M_NAME, ANAME_SZ) == 0) &&
      (strncmp (p->instance, KERB_M_INST, INST_SZ) == 0)) {
    bcopy((char *)new_master_key, (char *) key, sizeof (des_cblock));
    (p->key_version)++;
  } else {
    bcopy((char *)&(p->key_low), (char *)key, 4);
    bcopy((char *)&(p->key_high), (char *) (((long *) key) + 1), 4);
    kdb_encrypt_key (key, key, master_key, master_key_schedule, DECRYPT);
  }

  kdb_encrypt_key (key, key, new_master_key, new_master_key_schedule, ENCRYPT);

  bcopy((char *)key, (char *)&(p->key_low), 4);
  bcopy((char *)(((long *) key) + 1), (char *)&(p->key_high), 4);
  bzero((char *)key, sizeof (key));  /* a little paranoia ... */

  (p->kdc_key_ver)++;
}

void
convert_new_master_key (db_file, out)
     char *db_file;
     FILE *out;
{

  printf ("\n\nEnter the CURRENT master key.");
  if (kdb_get_master_key (TRUE, master_key, master_key_schedule) != 0) {
    fprintf (stderr, "get_master_key: Couldn't get master key.\n");
    clear_secrets ();
    exit (-1);
  }

  if (kdb_verify_master_key (master_key, master_key_schedule, stderr) < 0) {
    clear_secrets ();
    exit (-1);
  }

  printf ("\n\nNow enter the NEW master key.  Do not forget it!!");
  if (kdb_get_master_key (TRUE, new_master_key, new_master_key_schedule) != 0) {
    fprintf (stderr, "get_master_key: Couldn't get new master key.\n");
    clear_secrets ();
    exit (-1);
  }

  dump_db (db_file, out, convert_key_new_master);
}

void
convert_key_old_db (p)
     Principal *p;
{
  des_cblock key;

 /* leave null keys alone */
  if ((p->key_low == 0) && (p->key_high == 0)) return;

  bcopy((char *)&(p->key_low), (char *)key, 4);
  bcopy((char *)&(p->key_high), (char *)(((long *) key) + 1), 4);

#ifndef NOENCRYPTION
  des_pcbc_encrypt((des_cblock *)key,(des_cblock *)key,
	(long)sizeof(des_cblock),master_key_schedule,
	(des_cblock *)master_key_schedule,DECRYPT);
#endif

  /* make new key, new style */
  kdb_encrypt_key (key, key, master_key, master_key_schedule, ENCRYPT);

  bcopy((char *)key, (char *)&(p->key_low), 4);
  bcopy((char *)(((long *) key) + 1), (char *)&(p->key_high), 4);
  bzero((char *)key, sizeof (key));  /* a little paranoia ... */
}

void
convert_old_format_db (db_file, out)
     char *db_file;
     FILE *out;
{
  des_cblock key_from_db;
  Principal principal_data[1];
  int n, more;

  if (kdb_get_master_key (TRUE, master_key, master_key_schedule) != 0L) {
    fprintf (stderr, "verify_master_key: Couldn't get master key.\n");
    clear_secrets();
    exit (-1);
  }

  /* can't call kdb_verify_master_key because this is an old style db */
  /* lookup the master key version */
  n = kerb_get_principal(KERB_M_NAME, KERB_M_INST, principal_data,
			 1 /* only one please */, &more);
  if ((n != 1) || more) {
    fprintf(stderr, "verify_master_key: "
	    "Kerberos error on master key lookup, %d found.\n",
	    n);
    exit (-1);
  }

  /* set up the master key */
  fprintf(stderr, "Current Kerberos master key version is %d.\n",
	  principal_data[0].kdc_key_ver);

  /*
   * now use the master key to decrypt (old style) the key in the db, had better
   * be the same!
   */
  bcopy((char *)&principal_data[0].key_low, (char *)key_from_db, 4);
  bcopy((char *)&principal_data[0].key_high,
	(char *)(((long *) key_from_db) + 1), 4);
#ifndef NOENCRYPTION
  des_pcbc_encrypt((des_cblock *)key_from_db,(des_cblock *)key_from_db,
	(long)sizeof(key_from_db),master_key_schedule,
	(des_cblock *)master_key_schedule,DECRYPT);
#endif
  /* the decrypted database key had better equal the master key */
  n = bcmp((char *) master_key, (char *) key_from_db,
	   sizeof(master_key));
  bzero((char *)key_from_db, sizeof(key_from_db));

  if (n) {
    fprintf(stderr, "\n\07\07verify_master_key: Invalid master key, ");
    fprintf(stderr, "does not match database.\n");
    exit (-1);
  }

  fprintf(stderr, "Master key verified.\n");
  (void) fflush(stderr);

  dump_db (db_file, out, convert_key_old_db);
}

long
time_explode(cp)
register char *cp;
{
    char wbuf[5];
    struct tm tp;
    long maketime();
    int local;

    zaptime(&tp);			/* clear out the struct */

    if (strlen(cp) > 10) {		/* new format */
	(void) strncpy(wbuf, cp, 4);
	wbuf[4] = 0;
	tp.tm_year = atoi(wbuf);
	cp += 4;			/* step over the year */
	local = 0;			/* GMT */
    } else {				/* old format: local time,
					   year is 2 digits, assuming 19xx */
	wbuf[0] = *cp++;
	wbuf[1] = *cp++;
	wbuf[2] = 0;
	tp.tm_year = 1900 + atoi(wbuf);
	local = 1;			/* local */
    }

    wbuf[0] = *cp++;
    wbuf[1] = *cp++;
    wbuf[2] = 0;
    tp.tm_mon = atoi(wbuf)-1;

    wbuf[0] = *cp++;
    wbuf[1] = *cp++;
    tp.tm_mday = atoi(wbuf);

    wbuf[0] = *cp++;
    wbuf[1] = *cp++;
    tp.tm_hour = atoi(wbuf);

    wbuf[0] = *cp++;
    wbuf[1] = *cp++;
    tp.tm_min = atoi(wbuf);


    return(maketime(&tp, local));
}
