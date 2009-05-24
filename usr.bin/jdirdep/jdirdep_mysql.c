/* $FreeBSD$ */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <err.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "jdirdep.h"

static MYSQL *conn = NULL;
static char dbname[MAXPATHLEN];

void
jdirdep_db_close(void)
{
	if (conn != NULL)
		mysql_close(conn);
}

void *
jdirdep_db_command_res(const char *fmt, ...)
{
	MYSQL_RES *result;
	char *dbcmd;
	va_list ap;

	va_start(ap, fmt);

	if (vasprintf(&dbcmd, fmt, ap) == -1)
		errx(1, "Allocating and formatting string");

	if (mysql_query(conn, dbcmd)) {
		printf("Can't execute '%s': Error %u: %s\n", dbcmd, mysql_errno(conn), mysql_error(conn));
		mysql_close(conn);
		exit(1);
	}

	result = mysql_store_result(conn);

	free(dbcmd);

	va_end(ap);

	return(result);
}

void
jdirdep_db_command(db_cb_func func, void *vp, const char *fmt, ...)
{
	MYSQL_FIELD *fields;
	MYSQL_RES *result;
	MYSQL_ROW row;
	char *dbcmd;
	char *col[MAX_FIELDS];
	int i;
	int num_fields;
	va_list ap;

	va_start(ap, fmt);

	if (vasprintf(&dbcmd, fmt, ap) == -1)
		errx(1, "Allocating and formatting string");

	if (mysql_query(conn, dbcmd)) {
		printf("Can't execute '%s': Error %u: %s\n", dbcmd, mysql_errno(conn), mysql_error(conn));
		mysql_close(conn);
		exit(1);
	}

	result = mysql_store_result(conn);

	if (func != NULL && result != NULL) {
		num_fields = mysql_num_fields(result);

		if (num_fields >= (long) (sizeof(col) / sizeof(col[0]))) {
			printf("Query '%s': returned %d fields whereas %s is limited to %zd\n", dbcmd, num_fields, __func__, sizeof(col) / sizeof(col[0]));
			mysql_close(conn);
			exit(1);
		}

		fields = mysql_fetch_fields(result);

		for (i = 0; i < num_fields; i++)
			col[i] = fields[i].name;

		while ((row = mysql_fetch_row(result)))
			(*func)(vp, num_fields, (char **) row, col);

	}

	free(dbcmd);

	mysql_free_result(result);

	va_end(ap);
}

void
jdirdep_db_open(const char *name)
{
	MYSQL_RES *result;
	MYSQL_ROW row;
	struct passwd *pw;
	uint64_t dbid;
	struct utsname uts;

	if ((conn = mysql_init(NULL)) == NULL) {
		fprintf(stderr, "Can't init database connection: Error %d: %s\n", mysql_errno(conn), mysql_error(conn));
		exit(1);
	}

	if (mysql_real_connect(conn, "localhost", "root", "", NULL, 0, NULL, 0) == NULL) {
		printf("Can't connect to database server: Error %u: %s\n", mysql_errno(conn), mysql_error(conn));
		mysql_close(conn);
		exit(1);
	}

	if (uname(&uts) != 0)
		err(1, "Could not get uname info");

	jdirdep_db_command(NULL, NULL, "USE filedepdb;");

	if ((result = jdirdep_db_command_res("SELECT dbid FROM filedepidx WHERE dbname='%s:%s'", uts.nodename, name)) == NULL) {
		printf("Expected a result from a database query, but didn't get one\n");
		mysql_close(conn);
		exit(1);
	}

	if (mysql_num_rows(result) == 0) {
		if ((pw = getpwuid(getuid())) == NULL)
			err(1, "Could not get passwd info for uid %d", getuid());

		jdirdep_db_command(NULL, NULL, "INSERT INTO filedepidx VALUES ( NULL, '%s:%s', '%s' );", uts.nodename, name, pw->pw_name);
		dbid = mysql_insert_id(conn);
	} else {
		row = mysql_fetch_row(result);
		dbid = strtoll(row[0], NULL, 10);
	}

	mysql_free_result(result);

	snprintf(dbname, sizeof(dbname), "filedep%jd", dbid);

	jdirdep_db_command(NULL, NULL, "CREATE DATABASE IF NOT EXISTS %s;", dbname);

	jdirdep_db_command(NULL, NULL, "USE %s;", dbname);

	jdirdep_db_command(NULL, NULL, "CREATE TABLE IF NOT EXISTS reldir ( relid INT NOT NULL AUTO_INCREMENT, name TEXT(256) NOT NULL, PRIMARY KEY ( relid ), UNIQUE INDEX reldir_i1 ( name(256) ) );");
	jdirdep_db_command(NULL, NULL, "CREATE TABLE IF NOT EXISTS file ( filid INT NOT NULL AUTO_INCREMENT PRIMARY KEY, name TEXT(256) NOT NULL, relid INT, UNIQUE INDEX file_i1 ( name(256) ) );");
	jdirdep_db_command(NULL, NULL, "CREATE TABLE IF NOT EXISTS files_used ( filid INT NOT NULL, filid_using INT NOT NULL, UNIQUE INDEX files_used_i1 ( filid, filid_using ) );");
	jdirdep_db_command(NULL, NULL, "CREATE TABLE IF NOT EXISTS files_using ( filid INT NOT NULL, filid_used INT NOT NULL, UNIQUE INDEX files_using_i1 ( filid, filid_used ), UNIQUE INDEX files_using_i2 ( filid_used, filid ) );");
}

int64_t
jdirdep_db_rowid(void)
{
	return(mysql_insert_id(conn));
}
