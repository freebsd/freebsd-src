/*
 * $Id$ 
 *
 * Copyright 1987, 1988 by the Massachusetts Institute of Technology. 
 *
 * For copying and distribution information, please see the file
 * <mit-copyright.h>. 
 *
 * spm		Project Athena  8/85 
 *
 * This file defines data structures for the kerberos
 * authentication/authorization database. 
 *
 * They MUST correspond to those defined in *.rel 
 */

#ifndef KRB_DB_DEFS
#define KRB_DB_DEFS

#include <stdio.h>

#define KERB_M_NAME		"K"	/* Kerberos */
#define KERB_M_INST		"M"	/* Master */
#define KERB_DEFAULT_NAME	"default"
#define KERB_DEFAULT_INST	""
#ifndef DB_DIR
#define DB_DIR			"/var/db/kerberos"
#endif
#ifndef DBM_FILE
#define	DBM_FILE		DB_DIR "/principal"
#endif

/* this also defines the number of queue headers */
#define KERB_DB_HASH_MODULO 64


/* Arguments to kerb_dbl_lock() */

#define KERB_DBL_EXCLUSIVE 1
#define KERB_DBL_SHARED 0

/* arguments to kerb_db_set_lockmode() */

#define KERB_DBL_BLOCKING 0
#define KERB_DBL_NONBLOCKING 1

/* arguments to kdb_get_master_key */

#define KDB_GET_PROMPT 1
#define KDB_GET_TWICE  2

/* Principal defines the structure of a principal's name */

typedef struct {
    char    name[ANAME_SZ];
    char    instance[INST_SZ];

    u_int32_t key_low;
    u_int32_t key_high;
    u_int32_t exp_date;
    char    exp_date_txt[DATE_SZ];
    u_int32_t mod_date;
    char    mod_date_txt[DATE_SZ];
    u_int16_t attributes;
    u_int8_t max_life;
    u_int8_t kdc_key_ver;
    u_int8_t key_version;

    char    mod_name[ANAME_SZ];
    char    mod_instance[INST_SZ];
    char   *old;		/* cast to (Principal *); not in db,
				 * ptr to old vals */
} Principal;

typedef struct {
    int32_t    cpu;
    int32_t    elapsed;
    int32_t    dio;
    int32_t    pfault;
    int32_t    t_stamp;
    int32_t    n_retrieve;
    int32_t    n_replace;
    int32_t    n_append;
    int32_t    n_get_stat;
    int32_t    n_put_stat;
} DB_stat;

/* Dba defines the structure of a database administrator */

typedef struct {
    char    name[ANAME_SZ];
    char    instance[INST_SZ];
    u_int16_t attributes;
    u_int32_t exp_date;
    char    exp_date_txt[DATE_SZ];
    char   *old;	/*
			 * cast to (Dba *); not in db, ptr to
			 * old vals
			 */
} Dba;

typedef int (*k_iter_proc_t)(void*, Principal*);

void copy_from_key __P((des_cblock in, u_int32_t *lo, u_int32_t *hi));
void copy_to_key __P((u_int32_t *lo, u_int32_t *hi, des_cblock out));

void kdb_encrypt_key __P((des_cblock *, des_cblock *, des_cblock *,
			  des_key_schedule, int));
int kdb_get_master_key __P((int prompt, des_cblock *master_key,
			    des_key_schedule master_key_sched));
int kdb_get_new_master_key __P((des_cblock *, des_key_schedule, int));
int kdb_kstash __P((des_cblock *, char *));
int kdb_new_get_master_key __P((des_cblock *, des_key_schedule));
int kdb_new_get_new_master_key __P((des_cblock *key, des_key_schedule schedule, int verify));
long kdb_verify_master_key __P((des_cblock *, des_key_schedule, FILE *));
long *kerb_db_begin_update __P((void));
int kerb_db_create __P((char *db_name));
int kerb_db_delete_principal (char *name, char *inst);
void kerb_db_end_update __P((long *db));
int kerb_db_get_dba __P((char *, char *, Dba *, unsigned, int *));
void kerb_db_get_stat __P((DB_stat *));
int kerb_db_iterate __P((k_iter_proc_t, void*));
int kerb_db_put_principal __P((Principal *, unsigned int));
void kerb_db_put_stat __P((DB_stat *));
int kerb_db_rename __P((char *, char *));
int kerb_db_set_lockmode __P((int));
int kerb_db_set_name __P((char *));
int kerb_db_update __P((long *db, Principal *principal, unsigned int max));
int kerb_delete_principal __P((char *name, char *inst));
void kerb_fini __P((void));
int kerb_get_dba __P((char *, char *, Dba *, unsigned int, int *));
time_t kerb_get_db_age __P((void));
int kerb_get_principal __P((char *, char *, Principal *, unsigned int, int *));
int kerb_init __P((void));
int kerb_put_principal __P((Principal *, unsigned int));

#endif /* KRB_DB_DEFS */
