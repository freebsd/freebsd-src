/*
 * Copyright 1987, 1988 by the Massachusetts Institute of Technology.
 * 
 * For copying and distribution information, please see the file
 * <mit-copyright.h>.
 *
 * Kerberos database manipulation utility. This program allows you to
 * dump a kerberos database to an ascii readable file and load this
 * file into the database. Read locking of the database is done during a
 * dump operation. NO LOCKING is done during a load operation. Loads
 * should happen with other processes shutdown. 
 *
 * Written July 9, 1987 by Jeffrey I. Schiller
 */

#include "adm_locl.h"

RCSID("$Id: kdb_util.c,v 1.35 1997/05/07 00:57:45 assar Exp $");

static des_cblock master_key, new_master_key;
static des_key_schedule master_key_schedule, new_master_key_schedule;

#define zaptime(foo) memset((foo), 0, sizeof(*(foo)))

/* cv_key is a procedure which takes a principle and changes its key, 
   either for a new method of encrypting the keys, or a new master key.
   if cv_key is null no transformation of key is done (other than net byte
   order). */

struct callback_args {
    void (*cv_key)(Principal *);
    FILE *output_file;
};

static void
print_time(FILE *file, time_t timeval)
{
    struct tm *tm;
    tm = gmtime(&timeval);
    fprintf(file, " %04d%02d%02d%02d%02d",
	    tm->tm_year + 1900,
            tm->tm_mon + 1,
            tm->tm_mday,
            tm->tm_hour,
            tm->tm_min);
}

static long
time_explode(char *cp)
{
    char wbuf[5];
    struct tm tp;
    int local;

    zaptime(&tp);			/* clear out the struct */
    
    if (strlen(cp) > 10) {		/* new format */
	strncpy(wbuf, cp, 4);
	wbuf[4] = 0;
	tp.tm_year = atoi(wbuf) - 1900;
	cp += 4;			/* step over the year */
	local = 0;			/* GMT */
    } else {				/* old format: local time, 
					   year is 2 digits, assuming 19xx */
	wbuf[0] = *cp++;
	wbuf[1] = *cp++;
	wbuf[2] = 0;
	tp.tm_year = atoi(wbuf);
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


    return(tm2time(tp, local));
}

static int
dump_db_1(void *arg, Principal *principal)
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
    fprintf(a->output_file, "%s %s %d %d %d %d %x %x",
	    principal->name,
	    principal->instance,
	    principal->max_life,
	    principal->kdc_key_ver,
	    principal->key_version,
	    principal->attributes,
	    (int)htonl (principal->key_low),
	    (int)htonl (principal->key_high));
    print_time(a->output_file, principal->exp_date);
    print_time(a->output_file, principal->mod_date);
    fprintf(a->output_file, " %s %s\n",
	    principal->mod_name,
	    principal->mod_instance);
    return 0;
}

static int
dump_db (char *db_file, FILE *output_file, void (*cv_key) (Principal *))
{
    struct callback_args a;

    a.cv_key = cv_key;
    a.output_file = output_file;
    
    kerb_db_iterate ((k_iter_proc_t)dump_db_1, &a);
    return fflush(output_file);
}

static int
add_file(void *db, FILE *file)
{
    int ret;
    int lineno = 0;
    char line[1024];
    unsigned long key[2]; /* yes, long */
    Principal pr;
    
    char exp_date[64], mod_date[64];
    
    int life, kkvno, kvno;
    
    while(1){
	memset(&pr, 0, sizeof(pr));
	errno = 0;
	if(fgets(line, sizeof(line), file) == NULL){
	    if(errno != 0)
	      err (1, "fgets");
	    break;
	}
	lineno++;
	ret = sscanf(line, "%s %s %d %d %d %hd %lx %lx %s %s %s %s",
		     pr.name, pr.instance,
		     &life, &kkvno, &kvno,
		     &pr.attributes,
		     &key[0], &key[1],
		     exp_date, mod_date,
		     pr.mod_name, pr.mod_instance);
	if(ret != 12){
	    warnx("Line %d malformed (ignored)", lineno);
	    continue;
	}
	pr.key_low = ntohl (key[0]);
	pr.key_high = ntohl (key[1]);
	pr.max_life = life;
	pr.kdc_key_ver = kkvno;
	pr.key_version = kvno;
	pr.exp_date = time_explode(exp_date);
	pr.mod_date = time_explode(mod_date);
	if (pr.instance[0] == '*')
	    pr.instance[0] = 0;
	if (pr.mod_name[0] == '*')
	    pr.mod_name[0] = 0;
	if (pr.mod_instance[0] == '*')
	    pr.mod_instance[0] = 0;
	if (kerb_db_update(db, &pr, 1) != 1) {
	    warn ("store %s.%s aborted",
		  pr.name, pr.instance);
	    return 1;
	}
    }
    return 0;
}

static void
load_db (char *db_file, FILE *input_file)
{
    long *db;
    int     temp1;
    int code;
    char *temp_db_file;

    temp1 = strlen(db_file)+2;
    temp_db_file = malloc (temp1);
    strcpy(temp_db_file, db_file);
    strcat(temp_db_file, "~");

    /* Create the database */
    if ((code = kerb_db_create(temp_db_file)) != 0)
	err (1, "creating temp database %s", temp_db_file);
    kerb_db_set_name(temp_db_file);
    db = kerb_db_begin_update();
    if (db == NULL)
	err (1, "opening temp database %s", temp_db_file);
    
    if(add_file(db, input_file))
	errx (1, "Load aborted");

    kerb_db_end_update(db);
    if ((code = kerb_db_rename(temp_db_file, db_file)) != 0)
        warn("database rename failed");
    fclose(input_file);
    free(temp_db_file);
}

static void
merge_db(char *db_file, FILE *input_file)
{
    void *db;
    
    db = kerb_db_begin_update();
    if(db == NULL)
        err (1, "Couldn't open database");
    if(add_file(db, input_file))
        errx (1, "Merge aborted");
    kerb_db_end_update(db);
}

static void
update_ok_file (char *file_name)
{
    /* handle slave locking/failure stuff */
    char *file_ok;
    int fd;
    static char ok[]=".dump_ok";

    asprintf (&file_ok, "%s%s", file_name, ok);
    if (file_ok == NULL)
      errx (1, "out of memory");
    if ((fd = open(file_ok, O_WRONLY|O_CREAT|O_TRUNC, 0400)) < 0)
        err (1, "Error creating %s", file_ok);
    free(file_ok);
    close(fd);
}

static void
convert_key_new_master (Principal *p)
{
  des_cblock key;

  /* leave null keys alone */
  if ((p->key_low == 0) && (p->key_high == 0)) return;

  /* move current key to des_cblock for encryption, special case master key
     since that's changing */
  if ((strncmp (p->name, KERB_M_NAME, ANAME_SZ) == 0) &&
      (strncmp (p->instance, KERB_M_INST, INST_SZ) == 0)) {
    memcpy (key, new_master_key, sizeof(des_cblock));
    (p->key_version)++;
  } else {
    copy_to_key(&p->key_low, &p->key_high, key);
    kdb_encrypt_key (&key, &key, &master_key, master_key_schedule, DES_DECRYPT);
  }

  kdb_encrypt_key (&key, &key, &new_master_key, new_master_key_schedule, DES_ENCRYPT);

  copy_from_key(key, &(p->key_low), &(p->key_high));
  memset(key, 0, sizeof (key));  /* a little paranoia ... */

  (p->kdc_key_ver)++;
}

static void
clear_secrets (void)
{
  memset(master_key, 0, sizeof (des_cblock));
  memset(master_key_schedule, 0, sizeof (des_key_schedule));
  memset(new_master_key, 0, sizeof (des_cblock));
  memset(new_master_key_schedule, 0, sizeof (des_key_schedule));
}

static void
convert_new_master_key (char *db_file, FILE *out)
{
#ifdef RANDOM_MKEY
  errx (1, "Sorry, this function is not available with "
	"the new master key scheme.");
#else
  printf ("\n\nEnter the CURRENT master key.");
  if (kdb_get_master_key (KDB_GET_PROMPT, &master_key,
			  master_key_schedule) != 0) {
    clear_secrets ();
    errx (1, "Couldn't get master key.");
  }

  if (kdb_verify_master_key (&master_key, master_key_schedule, stderr) < 0) {
    clear_secrets ();
    exit (1);
  }

  printf ("\n\nNow enter the NEW master key.  Do not forget it!!");
  if (kdb_get_master_key (KDB_GET_TWICE, &new_master_key,
			  new_master_key_schedule) != 0) {
    clear_secrets ();
    errx (1, "Couldn't get new master key.");
  }

  dump_db (db_file, out, convert_key_new_master);
  {
    char fname[128];
    snprintf(fname, sizeof(fname), "%s.new", MKEYFILE);
    kdb_kstash(&new_master_key, fname);
  }
#endif /* RANDOM_MKEY */
}

static void
convert_key_old_db (Principal *p)
{
  des_cblock key;

 /* leave null keys alone */
  if ((p->key_low == 0) && (p->key_high == 0)) return;

  copy_to_key(&p->key_low, &p->key_high, key);

#ifndef NOENCRYPTION
  des_pcbc_encrypt((des_cblock *)key,(des_cblock *)key,
	(long)sizeof(des_cblock),master_key_schedule,
	(des_cblock *)master_key_schedule, DES_DECRYPT);
#endif

  /* make new key, new style */
  kdb_encrypt_key (&key, &key, &master_key, master_key_schedule, DES_ENCRYPT);

  copy_from_key(key, &(p->key_low), &(p->key_high));
  memset(key, 0, sizeof (key));  /* a little paranoia ... */
}

static void
convert_old_format_db (char *db_file, FILE *out)
{
  des_cblock key_from_db;
  Principal principal_data[1];
  int n, more;

  if (kdb_get_master_key (KDB_GET_PROMPT, &master_key,
			  master_key_schedule) != 0L) {
    clear_secrets();
    errx (1, "Couldn't get master key.");
  }

  /* can't call kdb_verify_master_key because this is an old style db */
  /* lookup the master key version */
  n = kerb_get_principal(KERB_M_NAME, KERB_M_INST, principal_data,
			 1 /* only one please */, &more);
  if ((n != 1) || more)
    errx (1, "verify_master_key: Kerberos error on master key lookup, %d found.\n", n);

  /* set up the master key */
  fprintf(stderr, "Current Kerberos master key version is %d.\n",
	  principal_data[0].kdc_key_ver);

  /*
   * now use the master key to decrypt (old style) the key in the db, had better
   * be the same! 
   */
  copy_to_key(&principal_data[0].key_low,
	      &principal_data[0].key_high,
	      key_from_db);
#ifndef NOENCRYPTION
  des_pcbc_encrypt(&key_from_db,&key_from_db,(long)sizeof(key_from_db),
	master_key_schedule,(des_cblock *)master_key_schedule, DES_DECRYPT);
#endif
  /* the decrypted database key had better equal the master key */

  n = memcmp(master_key, key_from_db, sizeof(master_key));
  memset(key_from_db, 0, sizeof(key_from_db));

  if (n) {
    fprintf(stderr, "\n\07\07verify_master_key: Invalid master key, ");
    fprintf(stderr, "does not match database.\n");
    exit (1);
  }
    
  fprintf(stderr, "Master key verified.\n");

  dump_db (db_file, out, convert_key_old_db);
}

int
main(int argc, char **argv)
{
    int ret;
    FILE   *file;
    enum {
	OP_LOAD,
	OP_MERGE,
	OP_DUMP,
	OP_SLAVE_DUMP,
	OP_NEW_MASTER,
	OP_CONVERT_OLD_DB
    }       op;
    char *file_name;
    char *db_name;

    set_progname (argv[0]);
    
    if (argc != 3 && argc != 4) {
	fprintf(stderr, "Usage: %s operation file [database name].\n",
		argv[0]);
	fprintf(stderr, "Operation is one of: "
		"load, merge, dump, slave_dump, new_master_key, "
		"convert_old_db\n");
	exit(1);
    }
    if (argc == 3)
	db_name = DBM_FILE;
    else
	db_name = argv[3];
    
    ret = kerb_db_set_name (db_name);
    
    /* this makes starting slave servers ~14.3 times easier */
    if(ret && strcmp(argv[1], "load") == 0)
       ret = kerb_db_create (db_name);

    if(ret)
      err (1, "Can't open database");

    if (!strcmp(argv[1], "load"))
	op = OP_LOAD;
    else if (!strcmp(argv[1], "merge"))
	op = OP_MERGE;
    else if (!strcmp(argv[1], "dump"))
	op = OP_DUMP;
    else if (!strcmp(argv[1], "slave_dump"))
        op = OP_SLAVE_DUMP;
    else if (!strcmp(argv[1], "new_master_key"))
        op = OP_NEW_MASTER;
    else if (!strcmp(argv[1], "convert_old_db"))
        op = OP_CONVERT_OLD_DB;
    else {
        warnx ("%s is an invalid operation.", argv[1]);
	warnx ("Valid operations are \"load\", \"merge\", "
	       "\"dump\", \"slave_dump\", \"new_master_key\", "
	       "and \"convert_old_db\"");
	return 1;
    }

    file_name = argv[2];
    file = fopen(file_name, (op == OP_LOAD || op == OP_MERGE) ? "r" : "w");
    if (file == NULL)
        err (1, "open %s", argv[2]);

    switch (op) {
    case OP_DUMP:
      if ((dump_db (db_name, file, (void (*)(Principal *)) 0) == EOF) ||
	  (fclose(file) == EOF))
	  err (1, "%s", file_name);
      break;
    case OP_SLAVE_DUMP:
      if ((dump_db (db_name, file, (void (*)(Principal *)) 0) == EOF) ||
	  (fclose(file) == EOF))
	err (1, "%s", file_name);
      update_ok_file (file_name);
      break;
    case OP_LOAD:
      load_db (db_name, file);
      break;
    case OP_MERGE:
      merge_db (db_name, file);
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
    return 0;
}
