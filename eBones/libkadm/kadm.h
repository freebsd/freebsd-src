/*
 * $Source: /afs/athena.mit.edu/astaff/project/kerberos/src/include/RCS/kadm.h,v $
 * $Author: jtkohl $
 * Header: /afs/athena.mit.edu/astaff/project/kerberos/src/include/RCS/kadm.h,v 4.2 89/09/26 09:15:20 jtkohl Exp 
 *
 * Copyright 1988 by the Massachusetts Institute of Technology.
 *
 * For copying and distribution information, please see the file
 * Copyright.MIT.
 *
 * Definitions for Kerberos administration server & client
 */

#ifndef KADM_DEFS
#define KADM_DEFS

/*
 * kadm.h
 * Header file for the fourth attempt at an admin server
 * Doug Church, December 28, 1989, MIT Project Athena
 */

/* for those broken Unixes without this defined... should be in sys/param.h */
#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 64
#endif

#include <sys/types.h>
#include <netinet/in.h>
#include <krb.h>
#include <des.h>

/* The global structures for the client and server */
typedef struct {
  struct sockaddr_in admin_addr;
  struct sockaddr_in my_addr;
  int my_addr_len;
  int admin_fd;			/* file descriptor for link to admin server */
  char sname[ANAME_SZ];		/* the service name */
  char sinst[INST_SZ];		/* the services instance */
  char krbrlm[REALM_SZ];
} Kadm_Client;

typedef struct {		/* status of the server, i.e the parameters */
   int inter;			/* Space for command line flags */
   char *sysfile;		/* filename of server */
} admin_params;			/* Well... it's the admin's parameters */

/* Largest password length to be supported */
#define MAX_KPW_LEN	128

/* Largest packet the admin server will ever allow itself to return */
#define KADM_RET_MAX 2048

/* That's right, versions are 8 byte strings */
#define KADM_VERSTR	"KADM0.0A"
#define KADM_ULOSE	"KYOULOSE"	/* sent back when server can't
					   decrypt client's msg */
#define KADM_VERSIZE strlen(KADM_VERSTR)

/* the lookups for the server instances */
#define PWSERV_NAME  "changepw"
#define KADM_SNAME   "kerberos_master"
#define KADM_SINST   "kerberos"

/* Attributes fields constants and macros */
#define ALLOC        2
#define RESERVED     3
#define DEALLOC      4
#define DEACTIVATED  5
#define ACTIVE       6

/* Kadm_vals structure for passing db fields into the server routines */
#define FLDSZ        4

typedef struct {
    u_char         fields[FLDSZ];     /* The active fields in this struct */
    char           name[ANAME_SZ];
    char           instance[INST_SZ];
    unsigned long  key_low;
    unsigned long  key_high;
    unsigned long  exp_date;
    unsigned short attributes;
    unsigned char  max_life;
} Kadm_vals;                    /* The basic values structure in Kadm */

/* Kadm_vals structure for passing db fields into the server routines */
#define FLDSZ        4

/* Need to define fields types here */
#define KADM_NAME       31
#define KADM_INST       30
#define KADM_EXPDATE    29
#define KADM_ATTR       28
#define KADM_MAXLIFE    27
#define KADM_DESKEY     26

/* To set a field entry f in a fields structure d */
#define SET_FIELD(f,d)  (d[3-(f/8)]|=(1<<(f%8)))

/* To set a field entry f in a fields structure d */
#define CLEAR_FIELD(f,d)  (d[3-(f/8)]&=(~(1<<(f%8))))

/* Is field f in fields structure d */
#define IS_FIELD(f,d)   (d[3-(f/8)]&(1<<(f%8)))

/* Various return codes */
#define KADM_SUCCESS    0

#define WILDCARD_STR "*"

enum acl_types {
ADDACL,
GETACL,
MODACL
};

/* Various opcodes for the admin server's functions */
#define CHANGE_PW    2
#define ADD_ENT      3
#define MOD_ENT      4
#define GET_ENT      5

extern long kdb_get_master_key();	/* XXX should be in krb_db.h */
extern long kdb_verify_master_key();	/* XXX ditto */

extern long krb_mk_priv(), krb_rd_priv(); /* XXX should be in krb.h */
extern void krb_set_tkt_string();	/* XXX ditto */

extern unsigned long quad_cksum();	/* XXX should be in des.h */

/* XXX This doesn't belong here!!! */
char *malloc(), *realloc();
#ifdef POSIX
typedef void sigtype;
#else
typedef int sigtype;
#endif

#endif KADM_DEFS
