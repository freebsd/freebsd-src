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
 * list and update contents of srvtab files
 */

/*
 * ksrvutil
 * list and update the contents of srvtab files
 */

#include "kadm_locl.h"

RCSID("$Id: ksrvutil.c,v 1.39 1997/05/02 14:28:52 assar Exp $");

#include "ksrvutil.h"

#ifdef NOENCRYPTION
#define read_long_pw_string placebo_read_pw_string
#else /* NOENCRYPTION */
#define read_long_pw_string des_read_pw_string
#endif /* NOENCRYPTION */

#define SRVTAB_MODE 0600	/* rw------- */
#define PAD "  "
#define VNO_HEADER "Version"
#define VNO_FORMAT "%4d   "
#define KEY_HEADER "       Key       " /* 17 characters long */
#define PRINC_HEADER "  Principal\n"
#define PRINC_FORMAT "%s"

char u_name[ANAME_SZ];
char u_inst[INST_SZ];
char u_realm[REALM_SZ];

int destroyp = FALSE;		/* Should the ticket file be destroyed? */

static unsigned short
get_mode(char *filename)
{
    struct stat statbuf;
    unsigned short mode;

    memset(&statbuf, 0, sizeof(statbuf));
    
    if (stat(filename, &statbuf) < 0) 
	mode = SRVTAB_MODE;
    else
	mode = statbuf.st_mode;

    return(mode);
}

static void
copy_keyfile(char *keyfile, char *backup_keyfile)
{
    int keyfile_fd;
    int backup_keyfile_fd;
    int keyfile_mode;
    char buf[BUFSIZ];		/* for copying keyfiles */
    int rcount;			/* for copying keyfiles */
    int try_again;
    
    memset(buf, 0, sizeof(buf));
    
    do {
	try_again = FALSE;
	if ((keyfile_fd = open(keyfile, O_RDONLY, 0)) < 0) {
	    if (errno != ENOENT)
	      err (1, "read %s", keyfile);
	    else {
		try_again = TRUE;
		if ((keyfile_fd = 
		     open(keyfile, 
			  O_WRONLY | O_TRUNC | O_CREAT, SRVTAB_MODE)) < 0)
		  err(1, "create %s", keyfile);
		else
		    if (close(keyfile_fd) < 0)
		      err (1, "close %s", keyfile);
	    }
	}
    } while(try_again);

    keyfile_mode = get_mode(keyfile);

    if ((backup_keyfile_fd = 
	 open(backup_keyfile, O_WRONLY | O_TRUNC | O_CREAT, 
	      keyfile_mode)) < 0)
	err (1, "write %s", backup_keyfile);
    do {
	if ((rcount = read(keyfile_fd, buf, sizeof(buf))) < 0)
	    err (1, "read %s", keyfile);
	if (rcount && (write(backup_keyfile_fd, buf, rcount) != rcount))
	    err (1, "write %s", backup_keyfile);
    } while (rcount);
    if (close(backup_keyfile_fd) < 0)
	err(1, "close %s", backup_keyfile);
    if (close(keyfile_fd) < 0)
	err(1, "close %s", keyfile);
}

void
leave(char *str, int x)
{
    if (str)
	fprintf(stderr, "%s\n", str);
    if (destroyp)
	 dest_tkt();
    exit(x);
}

void
safe_read_stdin(char *prompt, char *buf, size_t size)
{
    printf("%s", prompt);
    fflush(stdout);
    memset(buf, 0, size);
    if (read(0, buf, size - 1) < 0) {
	warn("read stdin");
	leave(NULL, 1);
    }
    buf[strlen(buf)-1] = 0;
}

void
safe_write(char *filename, int fd, void *buf, size_t len)
{
    if (write(fd, buf, len) != len) {
	warn("write %s", filename);
	close(fd);
	leave("In progress srvtab in this file.", 1);
    }
}

static int
yes_no(char *string, int dflt)
{
  char ynbuf[5];
  
  printf("%s (y,n) [%c]", string, dflt?'y':'n');
  for (;;) {
    safe_read_stdin("", ynbuf, sizeof(ynbuf));
    
    if ((ynbuf[0] == 'n') || (ynbuf[0] == 'N'))
      return(0);
    else if ((ynbuf[0] == 'y') || (ynbuf[0] == 'Y'))
      return(1);
    else if(ynbuf[0] == 0)
      return dflt;
    else {
      printf("Please enter 'y' or 'n': ");
      fflush(stdout);
    }
  }
}

int yn(char *string)
{
  return yes_no(string, 1);
}

int ny(char *string)
{
  return yes_no(string, 0);
}

static void
append_srvtab(char *filename, int fd, char *sname, char *sinst, char *srealm, unsigned char key_vno, unsigned char *key)
{
  /* Add one to append null */
    safe_write(filename, fd, sname, strlen(sname) + 1);
    safe_write(filename, fd, sinst, strlen(sinst) + 1);
    safe_write(filename, fd, srealm, strlen(srealm) + 1);
    safe_write(filename, fd, &key_vno, 1);
    safe_write(filename, fd, key, sizeof(des_cblock));
    fsync(fd);
}    

static void
print_key(unsigned char *key)
{
    int i;

    for (i = 0; i < 4; i++)
	printf("%02x", key[i]);
    printf(" ");
    for (i = 4; i < 8; i++)
	printf("%02x", key[i]);
}

static void
print_name(char *name, char *inst, char *realm)
{
    printf("%s", krb_unparse_name_long(name, inst, realm));
}

static int
get_svc_new_key(des_cblock *new_key, char *sname, char *sinst,
		char *srealm, char *keyfile)
{
    int status = KADM_SUCCESS;

    if (((status = krb_get_svc_in_tkt(sname, sinst, srealm, PWSERV_NAME,
				      KADM_SINST, 1, keyfile)) == KSUCCESS) &&
	((status = kadm_init_link(PWSERV_NAME, KRB_MASTER, srealm)) == 
	 KADM_SUCCESS)) {
#ifdef NOENCRYPTION
	memset(new_key, 0, sizeof(des_cblock));
	(*new_key)[0] = (unsigned char) 1;
#else /* NOENCRYPTION */
	des_new_random_key(new_key);
#endif /* NOENCRYPTION */
	return(KADM_SUCCESS);
    }
    
    return(status);
}

static void
get_key_from_password(des_cblock (*key), char *cellname)
{
    char password[MAX_KPW_LEN];	/* storage for the password */

    if (read_long_pw_string(password, sizeof(password)-1, "Password: ", 1))
	leave("Error reading password.", 1);

#ifdef NOENCRYPTION
    memset(key, 0, sizeof(des_cblock));
    (*key)[0] = (unsigned char) 1;
#else /* NOENCRYPTION */
    if (strlen(cellname) == 0)
      des_string_to_key(password, key);
    else
      afs_string_to_key(password, cellname, key);
#endif /* NOENCRYPTION */
    memset(password, 0, sizeof(password));
}    

static void
usage(void)
{
    fprintf(stderr, "Usage: ksrvutil [-f keyfile] [-i] [-k] ");
    fprintf(stderr, "[-p principal] [-r realm] ");
    fprintf(stderr, "[-c AFS cellname] ");
    fprintf(stderr, "{list | change | add | get}\n");
    fprintf(stderr, "   -i causes the program to ask for ");
    fprintf(stderr, "confirmation before changing keys.\n");
    fprintf(stderr, "   -k causes the key to printed for list or ");
    fprintf(stderr, "change.\n");
    exit(1);
}

int
main(int argc, char **argv)
{
    char sname[ANAME_SZ];	/* name of service */
    char sinst[INST_SZ];	/* instance of service */
    char srealm[REALM_SZ];	/* realm of service */
    unsigned char key_vno;	/* key version number */
    int status;			/* general purpose error status */
    des_cblock new_key;
    des_cblock old_key;
    char change_tkt[MaxPathLen]; /* Ticket to use for key change */
    char keyfile[MaxPathLen];	/* Original keyfile */
    char work_keyfile[MaxPathLen]; /* Working copy of keyfile */
    char backup_keyfile[MaxPathLen]; /* Backup copy of keyfile */
    unsigned short keyfile_mode; /* Protections on keyfile */
    int work_keyfile_fd = -1;	/* Initialize so that */
    int backup_keyfile_fd = -1;	/* compiler doesn't complain */
    char local_realm[REALM_SZ];	/* local kerberos realm */
    char cellname[1024];         /* AFS cell name */
    int c;
    int interactive = FALSE;
    int list = FALSE;
    int change = FALSE;
    int add = FALSE;
    int get = FALSE;
    int key = FALSE;		/* do we show keys? */
    int arg_entered = FALSE;
    int change_this_key = FALSE;
    char databuf[BUFSIZ];
    int first_printed = FALSE;	/* have we printed the first item? */
    
    memset(sname, 0, sizeof(sname));
    memset(sinst, 0, sizeof(sinst));
    memset(srealm, 0, sizeof(srealm));
    	  
    memset(change_tkt, 0, sizeof(change_tkt));
    memset(keyfile, 0, sizeof(keyfile));
    memset(work_keyfile, 0, sizeof(work_keyfile));
    memset(backup_keyfile, 0, sizeof(backup_keyfile));
    memset(local_realm, 0, sizeof(local_realm));
    memset(cellname, 0, sizeof(cellname));
    
    set_progname (argv[0]);

    if (krb_get_default_principal(u_name, u_inst, u_realm) < 0)
	errx (1, "could not get default principal");

    /* This is used only as a default for adding keys */
    if (krb_get_lrealm(local_realm, 1) != KSUCCESS)
	strcpy(local_realm, KRB_REALM);
    
    while((c = getopt(argc, argv, "ikc:f:p:r:")) != EOF) {
	 switch (c) {
	      case 'i':
	      interactive++;
	      break;
	      case 'k':
	      key++;
	      break;
	      case 'c':
	      strcpy(cellname, optarg);
	      break;
	      case 'f':
	      strcpy(keyfile, optarg);
	      break;
	      case 'p':
	      if((status = kname_parse (u_name, u_inst, u_realm, optarg)) !=
		 KSUCCESS)
		  errx (1, "principal %s: %s", optarg,
			krb_get_err_text(status));
	      break;
	      case 'r':
	      strcpy(u_realm, optarg);
	      break;
	      case '?':
	      usage();
	 }
    }
    if (optind >= argc)
	 usage();
    if (*u_realm == '\0')
	 strcpy (u_realm, local_realm);
    if (strcmp(argv[optind], "list") == 0) {
	 if (arg_entered)
	      usage();
	 else {
	      arg_entered++;
	      list++;
	 }
    }
    else if (strcmp(argv[optind], "change") == 0) {
	 if (arg_entered)
	      usage();
	 else {
	      arg_entered++;
	      change++;
	 }
    }
    else if (strcmp(argv[optind], "add") == 0) {
	 if (arg_entered)
	      usage();
	 else {
	      arg_entered++;
	      add++;
	 }
    }
    else if (strcmp(argv[optind], "get") == 0) {
	 if (arg_entered)
	      usage();
	 else {
	      arg_entered++;
	      get++;
	 }
    }
    else
	 usage();
    ++optind;
    
    if (!arg_entered)
	usage();

    if (!keyfile[0])
	strcpy(keyfile, KEYFILE);
    
    strcpy(work_keyfile, keyfile);
    strcpy(backup_keyfile, keyfile);
    
    if (change || add || get) {
	strcat(work_keyfile, ".work");
	strcat(backup_keyfile, ".old");
	
	copy_keyfile(keyfile, backup_keyfile);
    }
    
    if (add || get)
	copy_keyfile(backup_keyfile, work_keyfile);

    keyfile_mode = get_mode(keyfile);

    if (change || list)
	if ((backup_keyfile_fd = open(backup_keyfile, O_RDONLY, 0)) < 0)
	    err (1, "open %s", backup_keyfile);

    if (change) {
	if ((work_keyfile_fd = 
	     open(work_keyfile, O_WRONLY | O_CREAT | O_TRUNC, 
		  SRVTAB_MODE)) < 0)
	    err (1, "creat %s", work_keyfile);
    }
    else if (add) {
	if ((work_keyfile_fd =
	     open(work_keyfile, O_APPEND | O_WRONLY, SRVTAB_MODE)) < 0)
	    err (1, "open with append %s", work_keyfile );
    }
    else if (get) {
	if ((work_keyfile_fd =
	     open(work_keyfile, O_RDWR | O_CREAT, SRVTAB_MODE)) < 0)
	    err (1, "open for writing %s", work_keyfile);
    }
    
    if (change || list) {
	while ((getst(backup_keyfile_fd, sname, SNAME_SZ) > 0) &&
	       (getst(backup_keyfile_fd, sinst, INST_SZ) > 0) &&
	       (getst(backup_keyfile_fd, srealm, REALM_SZ) > 0) &&
	       (read(backup_keyfile_fd, &key_vno, 1) > 0) &&
	       (read(backup_keyfile_fd, old_key, sizeof(old_key)) > 0)) {
	    if (list) {
		if (!first_printed) {
		    printf(VNO_HEADER);
		    printf(PAD);
		    if (key) {
			printf(KEY_HEADER);
			printf(PAD);
		    }
		    printf(PRINC_HEADER);
		    first_printed = 1;
		}
		printf(VNO_FORMAT, key_vno);
		printf(PAD);
		if (key) {
		    print_key(old_key);
		    printf(PAD);
		}
		print_name(sname, sinst, srealm);
		printf("\n");
	    }
	    else if (change) {
		snprintf(change_tkt, sizeof(change_tkt),
			 TKT_ROOT "_ksrvutil.%u",
			 (unsigned)getpid());
		krb_set_tkt_string(change_tkt);
		destroyp = TRUE;

		printf("\nPrincipal: ");
		print_name(sname, sinst, srealm);
		printf("; version %d\n", key_vno);
		if (interactive)
		    change_this_key = yn("Change this key?");
		else if (change)
		    change_this_key = 1;
		else
		    change_this_key = 0;
		
		if (change_this_key)
		    printf("Changing to version %d.\n", key_vno + 1);
		else if (change)
		    printf("Not changing this key.\n");
		
		if (change_this_key) {
		    /*
		     * This is not a good choice of seed when/if the
		     * key has been compromised so we also use a
		     * random sequence number!
		     */
		    des_init_random_number_generator(&old_key);
		    {
		        des_cblock seqnum;
			des_generate_random_block(&seqnum);
			des_set_sequence_number((unsigned char *)&seqnum);
		    }
		    /* 
		     * Pick a new key and determine whether or not
		     * it is safe to change
		     */
		    if ((status = 
			 get_svc_new_key(&new_key, sname, sinst, 
					 srealm, keyfile)) == KADM_SUCCESS)
			key_vno++;
		    else {
		        memcpy(new_key, old_key, sizeof(new_key));
			warnx ("Key NOT changed: %s\n",
			       krb_get_err_text(status));
			change_this_key = FALSE;
		    }
		}
		else 
		    memcpy(new_key, old_key, sizeof(new_key));
		append_srvtab(work_keyfile, work_keyfile_fd, 
			      sname, sinst, srealm, key_vno, new_key);
		if (key && change_this_key) {
		    printf("Old key: ");
		    print_key(old_key);
		    printf("; new key: ");
		    print_key(new_key);
		    printf("\n");
		}
		if (change_this_key) {
		    if ((status = kadm_change_pw(new_key)) == KADM_SUCCESS) {
			printf("Key changed.\n");
			dest_tkt();
		    }
		    else {
			com_err(__progname, status, 
				" attempting to change password.");
			dest_tkt();
			/* XXX This knows the format of a keyfile */
			if (lseek(work_keyfile_fd, -9, SEEK_CUR) >= 0) {
			    key_vno--;
			    safe_write(work_keyfile,
				       work_keyfile_fd, &key_vno, 1);
			    safe_write(work_keyfile, work_keyfile_fd,
				       old_key, sizeof(des_cblock));
			    fsync(work_keyfile_fd);
			    fprintf(stderr,"Key NOT changed.\n");
			} else {
			    warn ("Unable to revert keyfile");
			    leave("", 1);
			}
		    }
		}
	    }
	    memset(old_key, 0, sizeof(des_cblock));
	    memset(new_key, 0, sizeof(des_cblock));
	}
    }
    else if (add) {
	do {
	    do {
		safe_read_stdin("Name: ", databuf, sizeof(databuf));
		strncpy(sname, databuf, sizeof(sname) - 1);
		if (strchr(sname, '.') != 0) {
		  strcpy(sinst, strchr(sname, '.') + 1);
		  *(strchr(sname, '.')) = 0;
		} else {
		  safe_read_stdin("Instance: ", databuf, sizeof(databuf));
		  strncpy(sinst, databuf, sizeof(sinst) - 1);
		}
		safe_read_stdin("Realm: ", databuf, sizeof(databuf));
		strncpy(srealm, databuf, sizeof(srealm) - 1);
		safe_read_stdin("Version number: ", databuf, sizeof(databuf));
		key_vno = atoi(databuf);
		if (key_vno == 0)
			key_vno = 1; /* Version numbers are never 0 */
		if (!srealm[0])
		    strcpy(srealm, local_realm);
		printf("New principal: ");
		print_name(sname, sinst, srealm);
		printf("; version %d\n", key_vno);
	    } while (!yn("Is this correct?"));
	    get_key_from_password(&new_key, cellname);
	    if (key) {
		printf("Key: ");
		print_key(new_key);
		printf("\n");
	    }
	    append_srvtab(work_keyfile, work_keyfile_fd, 
			  sname, sinst, srealm, key_vno, new_key);
	    printf("Key successfully added.\n");
	} while (yn("Would you like to add another key?"));
    }
    else if (get) {
        ksrvutil_get(work_keyfile_fd, work_keyfile,
		     argc - optind, argv + optind);
    }

    if (change || list) 
	if (close(backup_keyfile_fd) < 0)
	    warn ("close %s", backup_keyfile);
    
    if (change || add || get) {
	if (close(work_keyfile_fd) < 0)
	    err (1, "close %s", work_keyfile);
	if (rename(work_keyfile, keyfile) < 0)
	    err (1, "rename(%s, %s)", work_keyfile, keyfile);
	chmod(backup_keyfile, keyfile_mode);
	chmod(keyfile, keyfile_mode);
	printf("Old keyfile in %s.\n", backup_keyfile);
    }
    return 0;
}
