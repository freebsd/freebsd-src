/*
 * (c) Copyright 1995 HEWLETT-PACKARD COMPANY
 * 
 * To anyone who acknowledges that this file is provided 
 * "AS IS" without any express or implied warranty:
 * permission to use, copy, modify, and distribute this 
 * file for any purpose is hereby granted without fee, 
 * provided that the above copyright notice and this 
 * notice appears in all copies, and that the name of 
 * Hewlett-Packard Company not be used in advertising or 
 * publicity pertaining to distribution of the software 
 * without specific, written prior permission.  Hewlett-
 * Packard Company makes no representations about the 
 * suitability of this software for any purpose.
 *
 */
/*
 * k5dcecon - Program to convert a K5 TGT to a DCE context,
 * for use with DFS and its PAG.
 * 
 * The program is designed to be called as a sub process, 
 * and return via stdout the name of the cache which implies 
 * the PAG which should be used. This program itself does not 
 * use the cache or PAG itself, so the PAG in the kernel for 
 * this program may not be set. 
 * 
 * The calling program can then use the name of the cache
 * to set the KRB5CCNAME and PAG for its self and its children. 
 *
 * If no ticket was passed, an attemplt to join an existing
 * PAG will be made. 
 * 
 * If a forwarded K5 TGT is passed in, either a new DCE 
 * context will be created, or an existing one will be updated.
 * If the same ticket was already used to create an existing
 * context, it will be joined instead. 
 * 
 * Parts of this program are based on k5dceauth,c which was
 * given to me by HP and by the k5dcelogin.c which I developed. 
 * A slightly different version of k5dcelogin.c, was added to
 * DCE 1.2.2
 * 
 * D. E. Engert 6/17/97 ANL
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <locale.h>
#include <pwd.h>
#include <string.h>
#include <time.h>

#include <errno.h>
#include "k5dce.h"

#include <dce/sec_login.h>
#include <dce/dce_error.h>
#include <dce/passwd.h>

/* #define DEBUG */
#if defined(DEBUG)
#define DEEDEBUG(A) fprintf(stderr,A); fflush(stderr)
#define DEEDEBUG2(A,B) fprintf(stderr,A,B); fflush(stderr)
#else
#define DEEDEBUG(A)
#define DEEDEBUG2(A,B)
#endif

#ifdef __hpux
#define seteuid(A)		setresuid(-1,A,-1);
#endif


int k5dcecreate (uid_t, char *, char*, krb5_creds **);
int k5dcecon (uid_t, char *, char *);
int k5dcegettgt (krb5_ccache *, char *, char *, krb5_creds **);
int k5dcematch (uid_t, char *, char *, off_t *, krb5_creds **);
int k5dcesession (uid_t, char *, krb5_creds **, int *,krb5_flags);


char *progname = "k5dcecon";
static time_t now;

#ifdef notdef
#ifdef _AIX
/*---------------------------------------------*/
 /* AIX with DCE 1.1 does not have the com_err in the libdce.a
  * do a half hearted job of substituting for it. 
  */ 
void com_err(char *p1, int code, ...) 
{
    int lst;
    dce_error_string_t  err_string;
    dce_error_inq_text(code, err_string, &lst);
    fprintf(stderr,"Error %d in %s: %s\n", code, p1, err_string );
}

/*---------------------------------------------*/
void krb5_init_ets()
{

}
#endif
#endif


/*------------------------------------------------*/
/* find a cache to use  for our new pag           */
/* Since there is no simple way to determine which
 * caches are associated with a pag, we will have
 * do look around and see what makes most sense on 
 * different systems. 
 * on a Solaris system, and in the DCE source, 
 * the pags always start with a 41. 
 * this is not true on the IBM, where there does not
 * appear to be any pattern. 
 * 
 * But since we are always certifing our creds when
 * they are received, we can us that fact, and look
 * at the first word of the associated data file
 * to see that it has a "5". If not don't use. 
 */

int k5dcesession(luid, pname, tgt, ppag, tflags)
  uid_t luid;
  char *pname;
  krb5_creds **tgt;
  int *ppag;
  krb5_flags tflags;
{
  DIR *dirp;
  struct dirent *direntp;
  off_t size;
  krb5_timestamp endtime;
  int better = 0;
  krb5_creds *xtgt;

  char prev_name[17] = "";  
  krb5_timestamp prev_endtime;
  off_t prev_size;
  u_long prev_pag = 0;

  char ccname[64] = "FILE:/opt/dcelocal/var/security/creds/";
 
  error_status_t st;
  sec_login_handle_t lcontext = 0;
  dce_error_string_t  err_string;
  int lst;

  DEEDEBUG2("k5dcesession looking for flags %8.8x\n",tflags);

  dirp = opendir("/opt/dcelocal/var/security/creds/");
  if (dirp == NULL) {
	return 1;
  }

  while ( (direntp = readdir( dirp )) != NULL ) {

/*  
 * (but root has the ffffffff which we are not interested in)
 */
    if (!strncmp(direntp->d_name,"dcecred_",8)
         && (strlen(direntp->d_name) == 16)) {

      /* looks like a cache name, lets do the stat, etc */

      strcpy(ccname+38,direntp->d_name);
      if (!k5dcematch(luid, pname, ccname, &size, &xtgt))  {

        /* its one of our caches, see if it is better  
         * i.e. the endtime is farther, and if the endtimes
         * are the same, take the larger, as he who has the 
         * most tickets wins.
         * it must also had the same set of flags at least
         * i.e. if the forwarded TGT is forwardable, this one must 
         * be as well.   
         */

        DEEDEBUG2("Cache:%s",direntp->d_name);
        DEEDEBUG2(" size:%d",size);
		DEEDEBUG2(" flags:%8.8x",xtgt->ticket_flags);
		DEEDEBUG2(" %s",ctime((time_t *)&xtgt->times.endtime));
 
        if ((xtgt->ticket_flags & tflags) == tflags ) {
          if (prev_name[0]) {
            if (xtgt->times.endtime > prev_endtime) {
              better = 1;
            } else if ((xtgt->times.endtime = prev_endtime) 
                  && (size > prev_size)){
              better = 1;
	        }
          } else {   /* the first */
            if (xtgt->times.endtime >= now) {
            better = 1;
	        }
          }
          if (better) {
            strcpy(prev_name, direntp->d_name);
	  	    prev_endtime = xtgt->times.endtime;
            prev_size = size;
            sscanf(prev_name+8,"%8X",&prev_pag);
			*tgt = xtgt;
            better = 0;
          }
        }
      } 
    }
  }
  (void)closedir( dirp );

  if (!prev_name[0])  
	 return 1; /* failed to find one */

   DEEDEBUG2("Best: %s\n",prev_name);

   if (ppag)
      *ppag = prev_pag;

   strcpy(ccname+38,prev_name);
   setenv("KRB5CCNAME",ccname,1);
 
   return(0);
}


/*----------------------------------------------*/
/* see if this cache is for this this principal */

int k5dcematch(luid, pname, ccname, sizep, tgt) 
  uid_t luid;
  char *pname;
  char *ccname;
  off_t *sizep;  /* size of the file */
  krb5_creds **tgt;
{

  krb5_ccache cache;
  struct stat stbuf;
  char ccdata[256];
  int fd;
  int status;

  /* DEEDEBUG2("k5dcematch called: cache=%s\n",ccname+38); */

  if (!strncmp(ccname,"FILE:",5)) {

    strcpy(ccdata,ccname+5);
    strcat(ccdata,".data");

    /* DEEDEBUG2("Checking the .data file for %s\n",ccdata); */

    if (stat(ccdata, &stbuf))
      return(1);
 
    if (stbuf.st_uid != luid)
      return(1);

    if ((fd = open(ccdata,O_RDONLY)) == -1)
      return(1);
    
    if ((read(fd,&status,4)) != 4) {
      close(fd);
      return(1);
    }
    
    /* DEEDEBUG2(".data file status = %d\n", status); */

    if (status != 5)
     return(1);

    if (stat(ccname+5, &stbuf))
      return(1);

    if (stbuf.st_uid != luid)
      return(1);

    *sizep = stbuf.st_size;
  }

  return(k5dcegettgt(&cache, ccname, pname, tgt));
}


/*----------------------------------------*/
/* k5dcegettgt - get the tgt from a cache */

int k5dcegettgt(pcache, ccname, pname, tgt)
  krb5_ccache *pcache;
  char *ccname;
  char *pname;
  krb5_creds **tgt;

{
  krb5_ccache cache;
  krb5_cc_cursor cur;
  krb5_creds creds;
  int code;
  int found = 1;
  krb5_principal princ;
  char *kusername;
  krb5_flags flags;
  char *sname, *realm, *tgtname = NULL;

  /* Since DCE does not expose much of the Kerberos interface,
   * we will have to use what we can. This means setting the 
   * KRB5CCNAME for each file we want to test
   * We will also not worry about freeing extra cache structures
   * as this this routine is also not exposed, and this should not 
   * effect this module. 
   * We should also free the creds contents, but that is not exposed
   * either. 
   */

  setenv("KRB5CCNAME",ccname,1);
  cache = NULL;
  *tgt = NULL;

  if (code = krb5_cc_default(pcache)) {
     com_err(progname, code, "while getting ccache");
     goto return2;
  }

  DEEDEBUG("Got cache\n");
  flags = 0;
  if (code = krb5_cc_set_flags(*pcache, flags)) {
    com_err(progname, code,"While setting flags"); 
    goto return2;
  }
  DEEDEBUG("Set flags\n");
  if (code = krb5_cc_get_principal(*pcache, &princ)) {
	com_err(progname, code, "While getting princ");
    goto return1;
  }
  DEEDEBUG("Got principal\n");
  if (code = krb5_unparse_name(princ, &kusername)) {
    com_err(progname, code, "While unparsing principal");
    goto return1;
  }

  DEEDEBUG2("Unparsed to \"%s\"\n", kusername);
  DEEDEBUG2("pname is \"%s\"\n", pname);
  if (strcmp(kusername, pname)) {
   DEEDEBUG("Principals not equal\n");
   goto return1;
  }
  DEEDEBUG("Principals equal\n");

  realm = strchr(pname,'@');
  realm++;

  if ((tgtname = malloc(9 + 2 * strlen(realm))) == 0) {
       fprintf(stderr,"Malloc failed for tgtname\n");
       goto return1;
  }

  strcpy(tgtname,"krbtgt/");
  strcat(tgtname,realm);
  strcat(tgtname,"@");
  strcat(tgtname,realm);
 
  DEEDEBUG2("Getting tgt %s\n", tgtname);
  if (code = krb5_cc_start_seq_get(*pcache, &cur)) {
    com_err(progname, code, "while starting to retrieve tickets");
    goto return1;
  }

  while (!(code = krb5_cc_next_cred(*pcache, &cur, &creds))) {
    krb5_creds *cred = &creds;

    if (code = krb5_unparse_name(cred->server, &sname)) {
      com_err(progname, code, "while unparsing server name");
      continue;
    }

    if (strncmp(sname, tgtname, strlen(tgtname)) == 0) {
      DEEDEBUG("FOUND\n");
      if (code = krb5_copy_creds(&creds, tgt)) {
        com_err(progname, code, "while copying TGT");
        goto return1;
      }
      found = 0;
      break;
    } 
    /* we should do a krb5_free_cred_contents(creds); */
  }

  if (code = krb5_cc_end_seq_get(*pcache, &cur)) {
    com_err(progname, code, "while finishing retrieval"); 
    goto return2;
  }

return1:
  flags = KRB5_TC_OPENCLOSE; 
  krb5_cc_set_flags(*pcache, flags); /* force a close */
   
return2:
  if (tgtname)
    free(tgtname);

  return(found);
}


/*------------------------------------------*/
/* Convert a forwarded TGT to a DCE context */
int k5dcecon(luid, luser, pname)
  uid_t luid;
  char *luser;
  char *pname;
{

  krb5_creds *ftgt = NULL;
  krb5_creds *tgt = NULL;
  unsigned32 dfspag;
  boolean32 reset_passwd = 0;
  int lst;
  dce_error_string_t  err_string;
  char *shell_prog;
  krb5_ccache fcache;
  char *ccname;
  char *kusername;
  char *urealm;
  char *cp;
  int pag;
  int code;
  krb5_timestamp endtime;


  /* If there is no cache to be converted, we should not be here */

  if ((ccname = getenv("KRB5CCNAME")) == NULL) {
    DEEDEBUG("No KRB5CCNAME\n");
    return(1);
  }

  if (k5dcegettgt(&fcache, ccname, pname, &ftgt)) {
    fprintf(stderr, "%s: Did not find TGT\n", progname);
    return(1);
  }

 
  DEEDEBUG2("flags=%x\n",ftgt->ticket_flags);
  if (!(ftgt->ticket_flags & TKT_FLG_FORWARDABLE)){
    fprintf(stderr,"Ticket not forwardable\n");
    return(0); /* but OK to continue */
  }

  setenv("KRB5CCNAME","",1);
    
#define TKT_ACCEPTABLE (TKT_FLG_FORWARDABLE | TKT_FLG_PROXIABLE \
         | TKT_FLG_MAY_POSTDATE | TKT_FLG_RENEWABLE | TKT_FLG_HW_AUTH \
         | TKT_FLG_PRE_AUTH)

  if (!k5dcesession(luid, pname, &tgt, &pag, 
        (ftgt->ticket_flags & TKT_ACCEPTABLE))) {
    if (ftgt->times.endtime > tgt->times.endtime) {
      DEEDEBUG("Updating existing cache\n"); 
      return(k5dceupdate(&ftgt, pag));
    } else {
      DEEDEBUG("Using existing cache\n");
      return(0); /* use the original one */
    }
  } 
    /* see if the tgts match up */

  if ((code = k5dcecreate(luid, luser, pname, &ftgt))) {
	return (code);
  }

  /*
   * Destroy the Kerberos5 cred cache file.
   * but dont care aout the return code. 
   */

  DEEDEBUG("Destroying the old cache\n");
  if ((code = krb5_cc_destroy(fcache))) {
    com_err(progname, code, "while destroying Kerberos5 ccache");
  }
  return (0);
}


/*--------------------------------------------------*/
/* k5dceupdate - update the cache with a new TGT    */
/* Assumed that the KRB5CCNAME has been set         */

int k5dceupdate(krbtgt, pag) 
   krb5_creds **krbtgt;
   int pag;
{
 
  krb5_ccache ccache;
  int code;

  if (code = krb5_cc_default(&ccache)) {
    com_err(progname, code, "while opening cache for update");
    return(2);
   }

  if (code = ccache->ops->init(ccache,(*krbtgt)->client)) {
    com_err(progname, code, "while reinitilizing cache");
    return(3);
  } 

    /* krb5_cc_store_cred */
  if (code = ccache->ops->store(ccache, *krbtgt)) {
    com_err(progname, code, "while updating cache");
    return(2);
  }

  sec_login_pag_new_tgt(pag, (*krbtgt)->times.endtime);
  return(0);
}
/*--------------------------------------------------*/
/* k5dcecreate - create a new DCE context           */

int k5dcecreate(luid, luser, pname, krbtgt)
   uid_t luid;
   char *luser;
   char *pname;
   krb5_creds **krbtgt;
{
   
    char *cp;
    char *urealm;
    char *username;
    char *defrealm;
    uid_t uid;

    error_status_t st;
    sec_login_handle_t lcontext = 0;
    sec_login_auth_src_t auth_src = 0;
    boolean32 reset_passwd = 0;
    int lst;
    dce_error_string_t  err_string;

	setenv("KRB5CCNAME","",1); /* make sure it not misused */

	uid = getuid();
	DEEDEBUG2("uid=%d\n",uid);
    
	/* if run as root, change to user, so as to have the
	 * cache created for the local user even if cross-cell
	 * If run as a user, let standard file protection work.
	 */

	if (uid == 0) {
		seteuid(luid);
	}  

	cp = strchr(pname,'@');
	*cp = '\0';
	urealm = ++cp;

 DEEDEBUG2("basename=%s\n",cp);
 DEEDEBUG2("realm=%s\n",urealm);

    /* now build the username as a single string or a /.../cell/user
     * if this is a cross cell
     */

	if ((username = malloc(7+strlen(pname)+strlen(urealm))) == 0) {
         fprintf(stderr,"Malloc failed for username\n");
         goto abort;
    }
    if (krb5_get_default_realm(&defrealm)) {
        DEEDEBUG("krb5_get_default_realm failed\n");
        goto abort;
    }


    if (!strcmp(urealm,defrealm)) {
        strcpy(username,pname);
    } else {
        strcpy(username,"/.../");
        strcat(username,urealm);
        strcat(username,"/");
        strcat(username,pname);
    }

    /*
     * Setup a DCE login context
     */

    if (sec_login_setup_identity((unsigned_char_p_t)username, 
				 (sec_login_external_tgt|sec_login_proxy_cred),
				 &lcontext, &st)) {
	/*
	 * Add our TGT.
	 */
	  DEEDEBUG("Adding our new TGT\n");
	  sec_login_krb5_add_cred(lcontext, *krbtgt, &st);
	  if (st) {
	    dce_error_inq_text(st, err_string, &lst);
	    fprintf(stderr,
				"Error while adding credentials for %s because %s\n", 
				username, err_string);
	    goto abort;
	  }	
	  DEEDEBUG("validating and certifying\n");
	  /*
	   * Now "validate" and certify the identity,
	   *  usually we would pass a password here, but...
	   * sec_login_valid_and_cert_ident
	   * sec_login_validate_identity
	   */

	  if (sec_login_validate_identity(lcontext, 0, &reset_passwd,
		 &auth_src, &st)) {
	    DEEDEBUG2("validate_identity st=%d\n",st);
	    if (st) {
		  dce_error_inq_text(st, err_string, &lst);
		  fprintf(stderr, "Validation error for %s because %s\n",
				 username, err_string);
		  goto abort;
	    }
		if (!sec_login_certify_identity(lcontext,&st)) {
			dce_error_inq_text(st, err_string, &lst);
			fprintf(stderr,
			"Credentials not certified because %s\n",err_string);
		}
	    if (reset_passwd) {
		 fprintf(stderr,
                "Password must be changed for %s\n", username);
	    }
	    if (auth_src == sec_login_auth_src_local) {
		fprintf(stderr,
			 "Credentials obtained from local registry for %s\n", 
			 username);
	    }
	    if (auth_src == sec_login_auth_src_overridden) {
		  fprintf(stderr, "Validated %s from local override entry, no network credentials obtained\n", username);
		  goto abort; 

	    }
	    /*
	     * Actually create the cred files.
	     */
		DEEDEBUG("Ceating new cred files.\n");
	    sec_login_set_context(lcontext, &st);
	    if (st) {
		  dce_error_inq_text(st, err_string, &lst);
		  fprintf(stderr, 
                "Unable to set context for %s because %s\n",
		    username, err_string);
		  goto abort;
	    }

        /*
         * Now free up the local context and leave the 
         * network context with its pag
         */
#if 0
        sec_login_release_context(&lcontext, &st);
        if (st) {
          dce_error_inq_text(st, err_string, &lst);
          fprintf(stderr,
               "Unable to release context for %s because %s\n",
            username, err_string);
          goto abort;
        }
#endif
	}
	else {
	  DEEDEBUG2("validate failed %d\n",st);
	  dce_error_inq_text(st, err_string, &lst);
	  fprintf(stderr,
             "Unable to validate %s because %s\n", username, 
			err_string);
	  goto abort;
	}
  }
  else {
	dce_error_inq_text(st, err_string, &lst);
	fprintf(stderr, 
          "Unable to setup login entry for %s because %s\n",
       username, err_string);
	  goto abort;
  }

 done:
    /* if we were root, get back to root */

    DEEDEBUG2("sec_login_inq_pag %8.8x\n",
             sec_login_inq_pag(lcontext, &st));

    if (uid == 0) {
      seteuid(0);
    }  

	DEEDEBUG("completed\n");
	return(0);

 abort:
    if (uid == 0) {
      seteuid(0);
    }  

    DEEDEBUG("Aborting\n");
    return(2);
}



/*-------------------------------------------------*/
main(argc, argv)
  int argc;
  char *argv[];
{
  int status;
  extern int optind;
  extern char *optarg;
  int rv;

  char *lusername = NULL;
  char *pname = NULL;
  int fflag = 0;
  struct passwd *pw;
  uid_t luid;
  uid_t myuid;
  char *ccname;
  krb5_creds *tgt = NULL;

#ifdef DEBUG
  close(2);
  open("/tmp/k5dce.debug",O_WRONLY|O_CREAT|O_APPEND, 0600);
#endif

  if (myuid = getuid()) {
    DEEDEBUG2("UID = %d\n",myuid);
    exit(33); /* must be root to run this, get out now */
  }

  while ((rv = getopt(argc,argv,"l:p:fs")) != -1) {
    DEEDEBUG2("Arg = %c\n", rv);
    switch(rv) {
      case 'l':         /* user name */
	lusername = optarg;
	DEEDEBUG2("Optarg = %s\n", optarg);
	break;
      case 'p':         /* principal name */
        pname = optarg; 
	DEEDEBUG2("Optarg = %s\n", optarg);
        break;
      case 'f':         /* convert a forwarded TGT to a context */
        fflag++;
        break;
      case 's':      /* old test parameter, ignore it */
        break; 
    }
  }

  setlocale(LC_ALL, "");
  krb5_init_ets();
  time(&now); /* set time to check expired tickets */

  /* if lusername == NULL, Then user is passed as the USER= variable */ 

  if (!lusername) {
    lusername = getenv("USER");
    if (!lusername) {
      fprintf(stderr, "USER not in environment\n");
      return(3);
    }
  }

  if ((pw = getpwnam(lusername)) == NULL) {
    fprintf(stderr, "Who are you?\n");
    return(44);
  }

  luid = pw->pw_uid;

  if (fflag) {  
    status = k5dcecon(luid, lusername, pname); 
  } else {
    status = k5dcesession(luid, pname, &tgt, NULL, 0);
  }
 
  if (!status) {
    printf("%s",getenv("KRB5CCNAME")); /* return via stdout to caller */
    DEEDEBUG2("KRB5CCNAME=%s\n",getenv("KRB5CCNAME"));
  }

  DEEDEBUG2("Returning status %d\n",status);
  return (status);
}
