/* $FreeBSD$ */

#if defined(USE_SQLITE3)
#include <sqlite3.h>
#elif defined(USE_MYSQL)
#include <my_global.h>
#include <mysql.h>
#endif

#define JDIRDEP_OPT_ADD		(1 << 0)
#define JDIRDEP_OPT_DB		(1 << 1)
#define JDIRDEP_OPT_FORCE	(1 << 2)
#define JDIRDEP_OPT_GRAPH	(1 << 3)
#define JDIRDEP_OPT_META	(1 << 4)
#define JDIRDEP_OPT_RECURSE	(1 << 5)
#define JDIRDEP_OPT_SOURCE	(1 << 6)
#define JDIRDEP_OPT_UPDATE	(1 << 7)

#define MAX_FIELDS	20

typedef int (*db_cb_func)(void *, int, char **, char **);

int jdirdep(const char *srctop, const char *curdir, const char *srcrel, const char *objroot,
    const char *objdir, const char *sharedobj, const char *filedep_name,
    const char *meta_created, int options);
int64_t jdirdep_db_rowid(void);
void jdirdep_db_close(void);
void jdirdep_db_command(db_cb_func, void *, const char *, ...);
void *jdirdep_db_command_res(const char *, ...);
void jdirdep_db_open(const char *);
void jdirdep_incmk(const char *);
void jdirdep_supmac(const char *);
void jdirdep_supmac_add(const char *machine, const char *machine_arch);
