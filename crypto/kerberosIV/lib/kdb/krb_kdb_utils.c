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
 * Utility routines for Kerberos programs which directly access
 * the database.  This code was duplicated in too many places
 * before I gathered it here.
 *
 * Jon Rochlis, MIT Telecom, March 1988
 */

#include "kdb_locl.h"

#include <kdc.h>

RCSID("$Id: krb_kdb_utils.c,v 1.25 1999/03/13 21:24:21 assar Exp $");

/* always try /.k for backwards compatibility */
static char *master_key_files[] = { MKEYFILE, "/.k", NULL };

#ifdef HAVE_STRERROR
#define k_strerror(e) strerror(e)
#else
static
char *
k_strerror(int eno)
{
  extern int sys_nerr;
  extern char *sys_errlist[];

  static char emsg[128];

  if (eno < 0 || eno >= sys_nerr)
    snprintf(emsg, sizeof(emsg), "Error %d occurred.", eno);
  else
    return sys_errlist[eno];

  return emsg;
}
#endif

int
kdb_new_get_master_key(des_cblock *key, des_key_schedule schedule)
{
  int kfile = -1;
  int i;
  char buf[1024];

  char **mkey;

  for(mkey = master_key_files; *mkey; mkey++){
      kfile = open(*mkey, O_RDONLY);
      if(kfile < 0 && errno != ENOENT)
	  fprintf(stderr, "Failed to open master key file \"%s\": %s\n", 
		  *mkey,
		  k_strerror(errno));
      if(kfile >= 0)
	  break;
  }
  if(*mkey){
      int bytes;
      bytes = read(kfile, (char*)key, sizeof(des_cblock));
      close(kfile);
      if(bytes == sizeof(des_cblock)){
	  des_key_sched(key, schedule);
	  return 0;
      }
      fprintf(stderr, "Could only read %d bytes from master key file %s\n", 
	      bytes, *mkey);
  }else{
      fprintf(stderr, "No master key file found.\n");
  }

  
  i=0;
  while(i < 3){
      if(des_read_pw_string(buf, sizeof(buf), "Enter master password: ", 0))
	  break;

      /* buffer now contains either an old format master key password or a
       * new format base64 encoded master key
       */
      
      /* try to verify as old password */
      des_string_to_key(buf, key);
      des_key_sched(key, schedule);
      
      if(kdb_verify_master_key(key, schedule, NULL) != -1){
	  memset(buf, 0, sizeof(buf));
	  return 0;
      }
      
      /* failed test, so must be base64 encoded */
      
      if(base64_decode(buf, key) == 8){
	  des_key_sched(key, schedule);
	  if(kdb_verify_master_key(key, schedule, NULL) != -1){
	      memset(buf, 0, sizeof(buf));
	      return 0;
	  }
      }
      
      memset(buf, 0, sizeof(buf));
      fprintf(stderr, "Failed to verify master key.\n");
      i++;
  }
  
  /* life sucks */
  fprintf(stderr, "You loose.\n");
  exit(1);
}

int
kdb_new_get_new_master_key(des_cblock *key,
			   des_key_schedule schedule, 
			   int verify)
{
#ifndef RANDOM_MKEY
  des_read_password(key, "\nEnter Kerberos master password: ", verify);
  printf ("\n");
#else
  char buf[1024];
  des_generate_random_block (key);
  des_key_sched(key, schedule);
  
  des_read_pw_string(buf, sizeof(buf), "Enter master key seed: ", 0);
  des_cbc_cksum((des_cblock*)buf, key, sizeof(buf), schedule, key);
  memset(buf, 0, sizeof(buf));
#endif
  des_key_sched(key, schedule);
  return 0;
}

int
kdb_get_master_key(int prompt,
		   des_cblock *master_key, 
		   des_key_schedule master_key_sched)
{
  int ask = (prompt == KDB_GET_TWICE);
#ifndef RANDOM_MKEY
  ask |= (prompt == KDB_GET_PROMPT);
#endif
  
  if(ask)
    kdb_new_get_new_master_key(master_key, master_key_sched, 
			       prompt == KDB_GET_TWICE);
  else
    kdb_new_get_master_key(master_key, master_key_sched);
  return 0;
}

int
kdb_kstash(des_cblock *master_key, char *file)
{
  int kfile;

  kfile = open(file, O_TRUNC | O_RDWR | O_CREAT, 0600);
  if (kfile < 0) {
    return -1;
  }
  if (write(kfile, master_key, sizeof(des_cblock)) != sizeof(des_cblock)) {
    close(kfile);
    return -1;
  }
  close(kfile);
  return 0;
}

/* The old algorithm used the key schedule as the initial vector which
   was byte order depedent ... */

void
kdb_encrypt_key (des_cblock (*in), des_cblock (*out),
		 des_cblock (*master_key),
		 des_key_schedule master_key_sched, int e_d_flag)
{

#ifdef NOENCRYPTION
  memcpy(out, in, sizeof(des_cblock));
#else
  des_pcbc_encrypt(in,out,(long)sizeof(des_cblock),master_key_sched,master_key,
		   e_d_flag);
#endif
}

/* The caller is reasponsible for cleaning up the master key and sched,
   even if we can't verify the master key */

/* Returns master key version if successful, otherwise -1 */

long 
kdb_verify_master_key (des_cblock *master_key,
		       des_key_schedule master_key_sched,
		       FILE *out) /* NULL -> no output */
{
  des_cblock key_from_db;
  Principal principal_data[1];
  int n, more = 0;
  long master_key_version;

  /* lookup the master key version */
  n = kerb_get_principal(KERB_M_NAME, KERB_M_INST, principal_data,
			 1 /* only one please */, &more);
  if ((n != 1) || more) {
    if (out != NULL) 
      fprintf(out,
	      "verify_master_key: %s, %d found.\n",
	      "Kerberos error on master key version lookup",
	      n);
    return (-1);
  }

  master_key_version = (long) principal_data[0].key_version;

  /* set up the master key */
  if (out != NULL)  /* should we punt this? */
    fprintf(out, "Current Kerberos master key version is %d.\n",
	    principal_data[0].kdc_key_ver);

  /*
   * now use the master key to decrypt the key in the db, had better
   * be the same! 
   */
  copy_to_key(&principal_data[0].key_low,
	      &principal_data[0].key_high,
	      key_from_db);
  kdb_encrypt_key (&key_from_db, &key_from_db, 
		   master_key, master_key_sched, DES_DECRYPT);

  /* the decrypted database key had better equal the master key */
  n = memcmp(master_key, key_from_db, sizeof(master_key));
  /* this used to zero the master key here! */
  memset(key_from_db, 0, sizeof(key_from_db));
  memset(principal_data, 0, sizeof (principal_data));

  if (n && (out != NULL)) {
    fprintf(out, "\n\07\07verify_master_key: Invalid master key; ");
    fprintf(out, "does not match database.\n");
  }
  if(n)
    return (-1);

  if (out != (FILE *) NULL) {
    fprintf(out, "\nMaster key entered.  BEWARE!\07\07\n");
    fflush(out);
  }

  return (master_key_version);
}
