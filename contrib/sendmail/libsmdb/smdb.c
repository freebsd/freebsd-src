/*
** Copyright (c) 1999-2000 Sendmail, Inc. and its suppliers.
**	All rights reserved.
**
** By using this file, you agree to the terms and conditions set
** forth in the LICENSE file which can be found at the top level of
** the sendmail distribution.
*/

#ifndef lint
static char id[] = "@(#)$Id: smdb.c,v 8.37.4.1 2000/05/25 18:56:09 gshapiro Exp $";
#endif /* ! lint */

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>


#include <sendmail/sendmail.h>
#include <libsmdb/smdb.h>

/*
** SMDB_MALLOC_DATABASE -- Allocates a database structure.
**
**	Parameters:
**		None
**
**	Returns:
**		An pointer to an allocated SMDB_DATABASE structure or
**		NULL if it couldn't allocate the memory.
*/

SMDB_DATABASE *
smdb_malloc_database()
{
	SMDB_DATABASE *db;

	db = (SMDB_DATABASE *) malloc(sizeof(SMDB_DATABASE));

	if (db != NULL)
		memset(db, '\0', sizeof(SMDB_DATABASE));

	return db;
}


/*
** SMDB_FREE_DATABASE -- Unallocates a database structure.
**
**	Parameters:
**		database -- a SMDB_DATABASE pointer to deallocate.
**
**	Returns:
**		None
*/

void
smdb_free_database(database)
	SMDB_DATABASE *database;
{
	if (database != NULL)
		free(database);
}


/*
** SMDB_OPEN_DATABASE -- Opens a database.
**
**	This opens a database. If type is SMDB_DEFAULT it tries to
**	use a DB1 or DB2 hash. If that isn't available, it will try
**	to use NDBM. If a specific type is given it will try to open
**	a database of that type.
**
**	Parameters:
**		database -- An pointer to a SMDB_DATABASE pointer where the
**			   opened database will be stored. This should
**			   be unallocated.
**		db_name -- The name of the database to open. Do not include
**			  the file name extension.
**		mode -- The mode to set on the database file or files.
**		mode_mask -- Mode bits that must match on an opened database.
**		sff -- Flags to safefile.
**		type -- The type of database to open. Supported types
**		       vary depending on what was compiled in.
**		user_info -- Information on the user to use for file
**			    permissions.
**		params -- Params specific to the database being opened.
**			 Only supports some DB hash options right now
**			 (see smdb_db_open() for details).
**
**	Returns:
**		SMDBE_OK -- Success.
**		Anything else is an error. Look up more info about the
**		error in the comments for the specific open() used.
*/

int
smdb_open_database(database, db_name, mode, mode_mask, sff, type, user_info,
		   params)
	SMDB_DATABASE **database;
	char *db_name;
	int mode;
	int mode_mask;
	long sff;
	SMDB_DBTYPE type;
	SMDB_USER_INFO *user_info;
	SMDB_DBPARAMS *params;
{
	int result;
	bool type_was_default = FALSE;

	if (type == SMDB_TYPE_DEFAULT)
	{
		type_was_default = TRUE;
#ifdef NEWDB
		type = SMDB_TYPE_HASH;
#else /* NEWDB */
# ifdef NDBM
		type = SMDB_TYPE_NDBM;
# endif /* NDBM */
#endif /* NEWDB */
	}

	if (type == SMDB_TYPE_DEFAULT)
		return SMDBE_UNKNOWN_DB_TYPE;

	if ((strncmp(type, SMDB_TYPE_HASH, SMDB_TYPE_HASH_LEN) == 0) ||
	    (strncmp(type, SMDB_TYPE_BTREE, SMDB_TYPE_BTREE_LEN) == 0))
	{
#ifdef NEWDB
		result = smdb_db_open(database, db_name, mode, mode_mask, sff,
				      type, user_info, params);
# ifdef NDBM
		if (result == ENOENT && type_was_default)
			type = SMDB_TYPE_NDBM;
		else
# endif /* NDBM */
			return result;
#else /* NEWDB */
		return SMDBE_UNSUPPORTED_DB_TYPE;
#endif /* NEWDB */
	}

	if (strncmp(type, SMDB_TYPE_NDBM, SMDB_TYPE_NDBM_LEN) == 0)
	{
#ifdef NDBM
		result = smdb_ndbm_open(database, db_name, mode, mode_mask,
					sff, type, user_info, params);
		return result;
#else /* NDBM */
		return SMDBE_UNSUPPORTED_DB_TYPE;
#endif /* NDBM */
	}

	return SMDBE_UNKNOWN_DB_TYPE;
}

/*
** SMDB_ADD_EXTENSION -- Adds an extension to a file name.
**
**	Just adds a . followed by a string to a db_name if there
**	is room and the db_name does not already have that extension.
**
**	Parameters:
**		full_name -- The final file name.
**		max_full_name_len -- The max length for full_name.
**		db_name -- The name of the db.
**		extension -- The extension to add.
**
**	Returns:
**		SMDBE_OK -- Success.
**		Anything else is an error. Look up more info about the
**		error in the comments for the specific open() used.
*/

int
smdb_add_extension(full_name, max_full_name_len, db_name, extension)
	char *full_name;
	int max_full_name_len;
	char *db_name;
	char *extension;
{
	int extension_len;
	int db_name_len;

	if (full_name == NULL || db_name == NULL || extension == NULL)
		return SMDBE_INVALID_PARAMETER;

	extension_len = strlen(extension);
	db_name_len = strlen(db_name);

	if (extension_len + db_name_len + 2 > max_full_name_len)
		return SMDBE_DB_NAME_TOO_LONG;

	if (db_name_len < extension_len + 1 ||
	    db_name[db_name_len - extension_len - 1] != '.' ||
	    strcmp(&db_name[db_name_len - extension_len], extension) != 0)
		snprintf(full_name, max_full_name_len, "%s.%s", db_name,
			 extension);
	else
		(void) strlcpy(full_name, db_name, max_full_name_len);

	return SMDBE_OK;
}

/*
**  SMDB_LOCK_FILE -- Locks the database file.
**
**	Locks the actual database file.
**
**	Parameters:
**		lock_fd -- The resulting descriptor for the locked file.
**		db_name -- The name of the database without extension.
**		mode -- The open mode.
**		sff -- Flags to safefile.
**		extension -- The extension for the file.
**
**	Returns:
**		SMDBE_OK -- Success, otherwise errno.
*/

int
smdb_lock_file(lock_fd, db_name, mode, sff, extension)
	int *lock_fd;
	char *db_name;
	int mode;
	long sff;
	char *extension;
{
	int result;
	char file_name[SMDB_MAX_NAME_LEN];

	result = smdb_add_extension(file_name, SMDB_MAX_NAME_LEN, db_name,
				    extension);
	if (result != SMDBE_OK)
		return result;

	*lock_fd = safeopen(file_name, mode & ~O_TRUNC, 0644, sff);
	if (*lock_fd < 0)
		return errno;

	return SMDBE_OK;
}

/*
**  SMDB_UNLOCK_FILE -- Unlocks a file
**
**	Unlocks a file.
**
**	Parameters:
**		lock_fd -- The descriptor for the locked file.
**
**	Returns:
**		SMDBE_OK -- Success, otherwise errno.
*/

int
smdb_unlock_file(lock_fd)
	int lock_fd;
{
	int result;

	result = close(lock_fd);
	if (result != 0)
		return errno;

	return SMDBE_OK;
}

/*
**  SMDB_SETUP_FILE -- Gets db file ready for use.
**
**	Makes sure permissions on file are safe and creates it if it
**	doesn't exist.
**
**	Parameters:
**		db_name -- The name of the database without extension.
**		extension -- The extension.
**		sff -- Flags to safefile.
**		mode_mask -- Mode bits that must match.
**		user_info -- Information on the user to use for file
**			    permissions.
**		stat_info -- A place to put the stat info for the file.
**	Returns:
**		SMDBE_OK -- Success, otherwise errno.
*/

int
smdb_setup_file(db_name, extension, mode_mask, sff, user_info, stat_info)
	char *db_name;
	char *extension;
	int mode_mask;
	long sff;
	SMDB_USER_INFO *user_info;
	struct stat *stat_info;
{
	int st;
	int result;
	char db_file_name[SMDB_MAX_NAME_LEN];

	result = smdb_add_extension(db_file_name, SMDB_MAX_NAME_LEN, db_name,
				    extension);
	if (result != SMDBE_OK)
		return result;

	st = safefile(db_file_name, user_info->smdbu_id,
		      user_info->smdbu_group_id, user_info->smdbu_name,
		      sff, mode_mask, stat_info);
	if (st != 0)
		return st;

	return SMDBE_OK;
}

/*
**  SMDB_FILECHANGED -- Checks to see if a file changed.
**
**	Compares the passed in stat_info with a current stat on
**	the passed in file descriptor. Check filechanged for
**	return values.
**
**	Parameters:
**		db_name -- The name of the database without extension.
**		extension -- The extension.
**		db_fd -- A file descriptor for the database file.
**		stat_info -- An old stat_info.
**	Returns:
**		SMDBE_OK -- Success, otherwise errno.
*/

int
smdb_filechanged(db_name, extension, db_fd, stat_info)
	char *db_name;
	char *extension;
	int db_fd;
	struct stat *stat_info;
{
	int result;
	char db_file_name[SMDB_MAX_NAME_LEN];

	result = smdb_add_extension(db_file_name, SMDB_MAX_NAME_LEN, db_name,
				    extension);
	if (result != SMDBE_OK)
		return result;

	result = filechanged(db_file_name, db_fd, stat_info);

	return result;
}
/*
** SMDB_PRINT_AVAILABLE_TYPES -- Prints the names of the available types.
**
**	Parameters:
**		None
**
**	Returns:
**		None
*/

void
smdb_print_available_types()
{
#ifdef NDBM
	printf("dbm\n");
#endif /* NDBM */
#ifdef NEWDB
	printf("hash\n");
	printf("btree\n");
#endif /* NEWDB */
}
/*
** SMDB_DB_DEFINITION -- Given a database type, return database definition
**
**	Reads though a structure making an association with the database
**	type and the required cpp define from sendmail/README.
**	List size is dynamic and must be NULL terminated.
**
**	Parameters:
**		type -- The name of the database type.
**
**	Returns:
**		definition for type, otherwise NULL.
*/

typedef struct
{
	SMDB_DBTYPE type;
	char *dbdef;
} dbtype;

static dbtype DatabaseDefs[] =
{
	{ SMDB_TYPE_HASH,	"NEWDB" },
	{ SMDB_TYPE_BTREE,	"NEWDB" },
	{ SMDB_TYPE_NDBM,	"NDBM"	},
	{ NULL,			"OOPS"	}
};

char *
smdb_db_definition(type)
	SMDB_DBTYPE type;
{
	dbtype *ptr = DatabaseDefs;

	while (ptr != NULL && ptr->type != NULL)
	{
		if (strcmp(type, ptr->type) == 0)
			return ptr->dbdef;
		ptr++;
	}
	return NULL;
}
