/*
 * $Source: /afs/net/project/krb4/src/admin/RCS/kdb_edit.c,v $
 * $Author: tytso $
 *
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

#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/file.h>

#ifdef NEED_TIME_H
#include <time.h>
#endif
#include <sys/time.h>

#include <des.h>
#include <krb.h>
#include <krb_db.h>
/* MKEYFILE is now defined in kdc.h */
#include <kdc.h>

char    prog[32];
char   *progname = prog;
int     nflag = 0;
int     debug = 0;
extern  int krb_debug;

Principal principal_data;

static C_Block master_key;
static Key_schedule master_key_schedule;
static long master_key_version;

static char realm[REALM_SZ];

void fatal_error(), cleanup();
void Usage();
void change_principal();

int main(argc, argv)
     int     argc;
     char   *argv[];
{
  int i;

  prog[sizeof prog - 1] = '\0';	/* make sure terminated */
  strncpy(prog, argv[0], sizeof prog - 1);	/* salt away invoking
						 * program */

  /* Assume a long is four bytes */
  if (sizeof(long) != 4) {
    fprintf(stderr, "%s: size of long is %d.\n", prog, sizeof(long));
    exit(-1);
  }
  while (--argc > 0 && (*++argv)[0] == '-')
    for (i = 1; argv[0][i] != '\0'; i++) {
      switch (argv[0][i]) {
	
	/* debug flag */
      case 'd':
	debug = 1;
	continue;

	/* debug flag */
      case 'l':
	krb_debug |= 1;
	continue;

      case 'n':		/* read MKEYFILE for master key */
	nflag = 1;
	continue;
	
      default:
	fprintf(stderr, "%s: illegal flag \"%c\"\n", progname, argv[0][i]);
	Usage();	/* Give message and die */
      }
    };

  if (krb_get_lrealm(realm, 1)) {
	  fprintf(stderr, "Couldn't get local realm information.\n");
	  fatal_error();
  }

  kerb_init();
  if (argc > 0) {
    if (kerb_db_set_name(*argv) != 0) {
      fprintf(stderr, "Could not open altername database name\n");
      fatal_error();
    }
  }

  if (kdb_get_master_key ((nflag == 0), 
			  master_key, master_key_schedule) != 0) {
    fprintf (stderr, "Couldn't read master key.\n");
    fatal_error();
  }

  if ((master_key_version = kdb_verify_master_key(master_key,
						  master_key_schedule,
						  stdout)) < 0)
	  fatal_error();

  des_init_random_number_generator(master_key);

  change_principal("krbtgt", realm);
  change_principal("changepw", KRB_MASTER);

  cleanup();

  printf("\nKerberos database updated successfully.  Note that all\n");
  printf("existing ticket-granting tickets have been invalidated.\n\n");

  return(0);
}

void change_principal(input_name, input_instance)
     char *input_name;
     char *input_instance;
{
    int     n, more;
    C_Block new_key;

    n = kerb_get_principal(input_name, input_instance, &principal_data,
			   1, &more);
    if (!n) {
      fprintf(stderr, "Can't find principal database for %s.%s.\n", 
	      input_name, input_instance);
      fatal_error();
    }
    if (more) {
      fprintf(stderr, "More than one entry for %s.%s.\n", input_name, 
	      input_instance);
      fatal_error();
    }
      
    des_new_random_key(new_key);

    /* seal it under the kerberos master key */
    kdb_encrypt_key (new_key, new_key, 
		     master_key, master_key_schedule,
				 ENCRYPT);
    memcpy(&principal_data.key_low, new_key, 4);
    memcpy(&principal_data.key_high, ((long *) new_key) + 1, 4);
    memset(new_key, 0, sizeof(new_key));

    principal_data.key_version++;

    if (kerb_put_principal(&principal_data, 1)) {
      fprintf(stderr, "\nError updating Kerberos database");
      fatal_error();
    }

    memset(&principal_data.key_low, 0, 4);
    memset(&principal_data.key_high, 0, 4);
}

void fatal_error()
{
	cleanup();
	exit(1);
}

void cleanup()
{

  memset(master_key, 0, sizeof(master_key));
  memset(master_key_schedule, 0, sizeof(master_key_schedule));
  memset(&principal_data, 0, sizeof(principal_data));
}

void Usage()
{
    fprintf(stderr, "Usage: %s [-n]\n", progname);
    exit(1);
}
