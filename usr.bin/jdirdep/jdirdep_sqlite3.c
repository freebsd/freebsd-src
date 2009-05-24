/* $FreeBSD$ */

#include <sys/param.h>
#include <sys/types.h>
#include <err.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "jdirdep.h"

typedef void (create_fn)(void);

struct db_names {
	const char *name;
	int found;
	create_fn *func;
};

static sqlite3 *db = NULL;

static void
reldir_table_create(void)
{
	jdirdep_db_command(NULL, NULL, "CREATE TABLE reldir ( relid INTEGER PRIMARY KEY, name TEXT );");

	jdirdep_db_command(NULL, NULL, "CREATE UNIQUE INDEX reldir_i1 ON reldir(name);");
}

static void
file_table_create(void)
{
	jdirdep_db_command(NULL, NULL, "CREATE TABLE file ( filid INTEGER PRIMARY KEY, name TEXT, relid INTEGER );");

	jdirdep_db_command(NULL, NULL, "CREATE UNIQUE INDEX file_i1 ON file(name);");
}

static void
files_used_table_create(void)
{
	jdirdep_db_command(NULL, NULL, "CREATE TABLE files_used ( filid INTEGER, filid_using INTEGER);");

	jdirdep_db_command(NULL, NULL, "CREATE UNIQUE INDEX files_used_i1 ON files_used(filid, filid_using);");
}

static void
files_using_table_create(void)
{
	jdirdep_db_command(NULL, NULL, "CREATE TABLE files_using ( filid INTEGER, filid_used INTEGER);");

	jdirdep_db_command(NULL, NULL, "CREATE UNIQUE INDEX files_using_i1 ON files_using(filid, filid_used);");

	jdirdep_db_command(NULL, NULL, "CREATE UNIQUE INDEX files_using_i2 ON files_using(filid_used, filid);");
}

static struct db_names db_names[] = {
	{ "reldir", 0, reldir_table_create },
	{ "file", 0, file_table_create },
	{ "files_used", 0, files_used_table_create },
	{ "files_using", 0, files_using_table_create }
};

static int
table_list(void *thing __unused, int argc, char **argv, char **colname __unused)
{
	size_t i;

	for (i = 0; argc > 0 && i < sizeof(db_names) / sizeof(db_names[0]); i++)
		if (strcmp(argv[0], db_names[i].name) == 0)
			 db_names[i].found = 1;

	return(0);
}

static int
db_check(void)
{
	int ret = 0;
	size_t i;

	jdirdep_db_command(func, NULL, "SELECT name FROM sqlite_master WHERE type='table' ORDER BY name;");

	for (i = 0; i < sizeof(db_names) / sizeof(db_names[0]); i++)
		if (db_names[i].found == 0 && db_names[i].func != NULL)
			db_names[i].func();

	return(ret);
}

void
jdirdep_db_close(void)
{
	if (db != NULL)
		sqlite3_close(db);
}

void *
jdirdep_db_command(db_cb_func func, void *vp, const char *fmt, ...)
{
	char *dbcmd;
	char *errmsg = NULL;
	int count = 0;
	int ret;
	va_list ap;

	va_start(ap, fmt);

	if (vasprintf(&dbcmd, fmt, ap) == -1)
		errx(1, "Allocating and formatting string");

	/*
	 * Execute the database command, and retry for 10 minutes if the database is
	 * locked.
	 */
	while ((ret = sqlite3_exec(db, dbcmd, func, vp, &errmsg)) == SQLITE_LOCKED) {
		if (count > 600)
			break;

		sleep(1);
		count++;
		sqlite3_free(errmsg);
	}

	if (ret != SQLITE_OK) {
		fprintf(stderr, "Error in db command '%s': %s\n", dbcmd, errmsg);
		sqlite3_free(errmsg);
		exit(1);
	}

	free(dbcmd);

	va_end(ap);

	return(NULL);
}

void
jdirdep_db_open(const char *name)
{
	if (sqlite3_open(name, &db) != 0) {
		fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		exit(1);
	}
}

int64_t
jdirdep_db_rowid(void)
{
	return(sqlite3_last_insert_rowid(db));
}
