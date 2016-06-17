/*
 * Based on cfs.h from Coda, but revamped for increased simplicity.
 * Linux modifications by Peter Braam, Aug 1996
 * Rewritten for InterMezzo
 */

#ifndef _PRESTO_HEADER_
#define _PRESTO_HEADER_


/* upcall.c */
#define SYNCHRONOUS 0
#define ASYNCHRONOUS 1

int lento_permit(int minor, int pathlen, int fsetnamelen, char *path, char *fset);
int lento_opendir(int minor, int pathlen, char *path, int async);
int lento_kml(int minor, unsigned int offset, unsigned int first_recno,
              unsigned int length, unsigned int last_recno, int namelen,
              char *fsetname);
int lento_open(int minor, int pathlen, char *path);
int lento_journal(int minor, char *page, int async);
int lento_release_permit(int minor, int cookie);

/*
 * Kernel <--> Lento communications.
 */
/* upcalls */
#define LENTO_PERMIT    1
#define LENTO_JOURNAL   2
#define LENTO_OPENDIR   3
#define LENTO_OPEN      4
#define LENTO_SIGNAL    5
#define LENTO_KML       6
#define LENTO_COOKIE    7

/*         Lento <-> Presto  RPC arguments       */
struct lento_up_hdr {
        unsigned int opcode;
        unsigned int unique;    /* Keep multiple outstanding msgs distinct */
        u_short pid;            /* Common to all */
        u_short uid;
};

/* This structure _must_ sit at the beginning of the buffer */
struct lento_down_hdr {
        unsigned int opcode;
        unsigned int unique;    
        unsigned int result;
};

/* lento_permit: */
struct lento_permit_in {
        struct lento_up_hdr uh;
        int pathlen;
        int fsetnamelen;
        char path[0];
};
struct lento_permit_out {
        struct lento_down_hdr dh;
};


/* lento_opendir: */
struct lento_opendir_in {
        struct lento_up_hdr uh;
        int async;
        int pathlen;
        char path[0];
};
struct lento_opendir_out {
        struct lento_down_hdr dh;
};


/* lento_kml: */
struct lento_kml_in {
        struct lento_up_hdr uh;
        unsigned int offset;
        unsigned int first_recno;
        unsigned int length;
        unsigned int last_recno;
        int namelen;
        char fsetname[0];
};

struct lento_kml_out {
        struct lento_down_hdr dh;
};


/* lento_open: */
struct lento_open_in {
        struct lento_up_hdr uh;
        int pathlen;
        char path[0];
};
struct lento_open_out {
    struct lento_down_hdr dh;
};

/* lento_response_cookie */
struct lento_response_cookie_in {
        struct lento_up_hdr uh;
        int cookie;
};

struct lento_response_cookie_out {
    struct lento_down_hdr dh;
};


struct lento_mknod {
  struct lento_down_hdr dh;
  int    major;
  int    minor;
  int    mode;
  char   path[0];
};


/* NB: every struct below begins with an up_hdr */
union up_args {
    struct lento_up_hdr uh;             
    struct lento_permit_in lento_permit;
    struct lento_open_in lento_open;
    struct lento_opendir_in lento_opendir;
    struct lento_kml_in lento_kml;
    struct lento_response_cookie_in lento_response_cookie;
};

union down_args {
    struct lento_down_hdr dh;
    struct lento_permit_out lento_permit;
    struct lento_open_out lento_open;
    struct lento_opendir_out lento_opendir;
    struct lento_kml_out lento_kml;
    struct lento_response_cookie_out lento_response_cookie;
};    

#include "intermezzo_psdev.h"

int lento_upcall(int minor, int read_size, int *rep_size, 
                 union up_args *buffer, int async,
                 struct upc_req *rq );
#endif 

