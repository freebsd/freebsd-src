/*
 * Copyright 1987, 1988 by the Massachusetts Institute of Technology.
 *
 *	from: ext_srvtab.c,v 4.1 89/07/18 16:49:30 jtkohl Exp $
 *	$FreeBSD$
 */

#if 0
#ifndef	lint
static char rcsid[] =
"$FreeBSD$";
#endif	lint
#endif

#include <stdio.h>
#include <string.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <des.h>
#include <krb.h>
#include <krb_db.h>

#define TRUE 1
#define FALSE 0

static C_Block master_key;
static C_Block session_key;
static Key_schedule master_key_schedule;
char progname[] = "ext_srvtab";
char realm[REALM_SZ];

void FWrite(char *p, int size, int n, FILE *f);
void StampOutSecrets(void);
void usage(void);

int
main(argc, argv)
  int argc;
  char *argv[];
{
    FILE *fout;
    char fname[1024];
    int fopen_errs = 0;
    int arg;
    Principal princs[40];
    int more;
    int prompt = TRUE;
    register int n, i;

    bzero(realm, sizeof(realm));

    /* Parse commandline arguments */
    if (argc < 2)
	usage();
    else {
	for (i = 1; i < argc; i++) {
	    if (strcmp(argv[i], "-n") == 0)
		prompt = FALSE;
	    else if (strcmp(argv[i], "-r") == 0) {
		if (++i >= argc)
		    usage();
		else {
		    strcpy(realm, argv[i]);
		    /*
		     * This is to humor the broken way commandline
		     * argument parsing is done.  Later, this
		     * program ignores everything that starts with -.
		     */
		    argv[i][0] = '-';
		}
	    }
	    else if (argv[i][0] == '-')
		usage();
	    else
		if (!k_isinst(argv[i])) {
		fprintf(stderr, "%s: bad instance name: %s\n",
			progname, argv[i]);
		usage();
	    }
	}
    }

    if (kdb_get_master_key (prompt, master_key, master_key_schedule) != 0) {
      fprintf (stderr, "Couldn't read master key.\n");
      fflush (stderr);
      exit(1);
    }

    if (kdb_verify_master_key (master_key, master_key_schedule, stderr) < 0) {
      exit(1);
    }

    /* For each arg, search for instances of arg, and produce */
    /* srvtab file */
    if (!realm[0])
	if (krb_get_lrealm(realm, 1) != KSUCCESS) {
	    fprintf(stderr, "%s: couldn't get local realm\n", progname);
	    exit(1);
	}
    (void) umask(077);

    for (arg = 1; arg < argc; arg++) {
	if (argv[arg][0] == '-')
	    continue;
	sprintf(fname, "%s-new-srvtab", argv[arg]);
	if ((fout = fopen(fname, "w")) == NULL) {
	    fprintf(stderr, "Couldn't create file '%s'.\n", fname);
	    fopen_errs++;
	    continue;
	}
	printf("Generating '%s'....\n", fname);
	n = kerb_get_principal("*", argv[arg], &princs[0], 40, &more);
	if (more)
	    fprintf(stderr, "More than 40 found...\n");
	for (i = 0; i < n; i++) {
	    FWrite(princs[i].name, strlen(princs[i].name) + 1, 1, fout);
	    FWrite(princs[i].instance, strlen(princs[i].instance) + 1,
		   1, fout);
	    FWrite(realm, strlen(realm) + 1, 1, fout);
	    FWrite(&princs[i].key_version,
		sizeof(princs[i].key_version), 1, fout);
	    bcopy(&princs[i].key_low, session_key, sizeof(long));
	    bcopy(&princs[i].key_high, session_key + sizeof(long),
		  sizeof(long));
	    kdb_encrypt_key (session_key, session_key,
			     master_key, master_key_schedule, DES_DECRYPT);
	    FWrite(session_key, sizeof session_key, 1, fout);
	}
	fclose(fout);
    }

    StampOutSecrets();

    exit(fopen_errs);		/* 0 errors if successful */

}

void
Die()
{
    StampOutSecrets();
    exit(1);
}

void
FWrite(p, size, n, f)
  char   *p;
  int     size;
  int     n;
  FILE   *f;
{
    if (fwrite(p, size, n, f) != n) {
	printf("Error writing output file.  Terminating.\n");
	Die();
    }
}

void
StampOutSecrets()
{
    bzero(master_key, sizeof master_key);
    bzero(session_key, sizeof session_key);
    bzero(master_key_schedule, sizeof master_key_schedule);
}

void
usage()
{
    fprintf(stderr,
	    "Usage: %s [-n] [-r realm] instance [instance ...]\n", progname);
    exit(1);
}
