/*
**  Copyright (c) 2018 Proofpoint, Inc. and its suppliers.
**	All rights reserved.
**
**  By using this file, you agree to the terms and conditions set
**  forth in the LICENSE file which can be found at the top level of
**  the sendmail distribution.
*/

#include <sm/gen.h>
SM_RCSID("@(#)$Id: smcdb.c,v 8.55 2013-11-22 20:51:49 ca Exp $")

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include <sendmail/sendmail.h>
#include <libsmdb/smdb.h>

#if CDB
#include <assert.h>
#include <cdb.h>

typedef struct cdb	cdb_map_T, *cdb_map_P;
typedef struct cdb_make	cdb_make_T, *cdb_make_P;
typedef union sm_cdbs_U sm_cdbs_T, *sm_cdbs_P;
union sm_cdbs_U
{
	cdb_map_T	 cdbs_cdb_rd;
	cdb_make_T	 cdbs_cdb_wr;
};

struct smdb_cdb_database
{
	sm_cdbs_T	cdbmap_map;
	int		cdbmap_fd;
	int		smcdb_lock_fd;
	bool		cdbmap_create;
	unsigned	smcdb_pos;
	int		smcdb_n;
};
typedef struct smdb_cdb_database SMDB_CDB_DATABASE;

/* static int smdb_type_to_cdb_type __P((SMDB_DBTYPE type)); */
static int cdb_error_to_smdb __P((int error));
static SMDB_CDB_DATABASE * smcdb_malloc_database __P((void));
static int smcdb_close __P((SMDB_DATABASE *database));
static int smcdb_del __P((SMDB_DATABASE *database, SMDB_DBENT *key, unsigned int flags));
static int smcdb_fd __P((SMDB_DATABASE *database, int *fd));
static int smcdb_lockfd __P((SMDB_DATABASE *database));
static int smcdb_get __P((SMDB_DATABASE *database, SMDB_DBENT *key, SMDB_DBENT *data, unsigned int flags));
static int smcdb_put __P((SMDB_DATABASE *database, SMDB_DBENT *key, SMDB_DBENT *data, unsigned int flags));
static int smcdb_set_owner __P((SMDB_DATABASE *database, uid_t uid, gid_t gid));
static int smcdb_sync __P((SMDB_DATABASE *database, unsigned int flags));
static int smcdb_cursor_close __P((SMDB_CURSOR *cursor));
static int smcdb_cursor_del __P((SMDB_CURSOR *cursor, SMDB_FLAG flags));
static int smcdb_cursor_get __P((SMDB_CURSOR *cursor, SMDB_DBENT *key, SMDB_DBENT *value, SMDB_FLAG flags));
static int smcdb_cursor_put __P((SMDB_CURSOR *cursor, SMDB_DBENT *key, SMDB_DBENT *value, SMDB_FLAG flags));
static int smcdb_cursor __P((SMDB_DATABASE *database, SMDB_CURSOR **cursor, SMDB_FLAG flags));

/*
**  SMDB_TYPE_TO_CDB_TYPE -- Translates smdb database type to cdb type.
**
**	Parameters:
**		type -- The type to translate.
**
**	Returns:
**		The CDB type that corresponsds to the passed in SMDB type.
**		Returns -1 if there is no equivalent type.
**
*/

# if 0
static int
smdb_type_to_cdb_type(type)
	SMDB_DBTYPE type;
{
	return 0;	/* XXX */
}
# endif

/*
**  CDB_ERROR_TO_SMDB -- Translates cdb errors to smdbe errors
**
**	Parameters:
**		error -- The error to translate.
**
**	Returns:
**		The SMDBE error corresponding to the cdb error.
**		If we don't have a corresponding error, it returns error.
**
*/

static int
cdb_error_to_smdb(error)
	int error;
{
	int result;

	switch (error)
	{
		case 0:
			result = SMDBE_OK;
			break;

		default:
			result = error;
	}
	return result;
}

SMDB_CDB_DATABASE *
smcdb_malloc_database()
{
	SMDB_CDB_DATABASE *cdb;

	cdb = (SMDB_CDB_DATABASE *) malloc(sizeof(SMDB_CDB_DATABASE));
	if (cdb != NULL)
		cdb->smcdb_lock_fd = -1;

	return cdb;
}

static int
smcdb_close(database)
	SMDB_DATABASE *database;
{
	int result, fd;
	SMDB_CDB_DATABASE *sm_cdbmap = (SMDB_CDB_DATABASE *) database->smdb_impl;

	if (NULL == sm_cdbmap)
		return -1;
	result = 0;
	if (sm_cdbmap->cdbmap_create)
		result = cdb_make_finish(&sm_cdbmap->cdbmap_map.cdbs_cdb_wr);

	fd = sm_cdbmap->cdbmap_fd;
	if (fd >= 0)
	{
		close(fd);
		sm_cdbmap->cdbmap_fd = -1;
	}

	free(sm_cdbmap);
	database->smdb_impl = NULL;

	return result;
}

static int
smcdb_del(database, key, flags)
	SMDB_DATABASE *database;
	SMDB_DBENT *key;
	unsigned int flags;
{
	SMDB_CDB_DATABASE *sm_cdbmap = (SMDB_CDB_DATABASE *) database->smdb_impl;

	assert(sm_cdbmap != NULL);
	return -1;
}

static int
smcdb_fd(database, fd)
	SMDB_DATABASE *database;
	int *fd;
{
	SMDB_CDB_DATABASE *sm_cdbmap = (SMDB_CDB_DATABASE *) database->smdb_impl;
	return sm_cdbmap->cdbmap_fd;
}

static int
smcdb_lockfd(database)
	SMDB_DATABASE *database;
{
	SMDB_CDB_DATABASE *sm_cdbmap = (SMDB_CDB_DATABASE *) database->smdb_impl;

	return sm_cdbmap->smcdb_lock_fd;
}

/*
**  allocate/free: who does it: caller or callee?
**  If this code does it: the "last" entry will leak.
*/

#define DBEALLOC(dbe, l)	\
	do	\
	{	\
		if ((dbe)->size > 0 && l > (dbe)->size)	\
		{	\
			free((dbe)->data);	\
			(dbe)->size = 0;	\
		}	\
		if ((dbe)->size == 0)	\
		{	\
			(dbe)->data = malloc(l);	\
			if ((dbe)->data == NULL)	\
				return SMDBE_MALLOC;	\
			(dbe)->size = l;	\
		}	\
		if (l > (dbe)->size)	\
			return SMDBE_MALLOC;	/* XXX bogus */	\
	} while (0)


static int
smcdb_get(database, key, data, flags)
	SMDB_DATABASE *database;
	SMDB_DBENT *key;
	SMDB_DBENT *data;
	unsigned int flags;
{
	SMDB_CDB_DATABASE *sm_cdbmap = (SMDB_CDB_DATABASE *) database->smdb_impl;
	size_t l;
	int ret;

	ret = SM_SUCCESS;

	if (NULL == sm_cdbmap )
		return -1;
	/* SM_ASSERT(!sm_cdbmap->cdbmap_create); */

	/* need to lock access? single threaded access! */
	ret = cdb_find(&sm_cdbmap->cdbmap_map.cdbs_cdb_rd,
			key->data, key->size);
	if (ret > 0)
	{
		l = cdb_datalen(&sm_cdbmap->cdbmap_map.cdbs_cdb_rd);
		DBEALLOC(data, l);
		ret = cdb_read(&sm_cdbmap->cdbmap_map.cdbs_cdb_rd,
				data->data, l,
				cdb_datapos(&sm_cdbmap->cdbmap_map.cdbs_cdb_rd));
		if (ret < 0)
			ret = -1;
		else
		{
			data->size = l;
			ret = SM_SUCCESS;
		}
	}
	else
		ret = -1;

	return ret;
}

static int
smcdb_put(database, key, data, flags)
	SMDB_DATABASE *database;
	SMDB_DBENT *key;
	SMDB_DBENT *data;
	unsigned int flags;
{
	int r, cdb_flags;
	SMDB_CDB_DATABASE *sm_cdbmap = (SMDB_CDB_DATABASE *) database->smdb_impl;

	assert(sm_cdbmap != NULL);
	if (bitset(SMDBF_NO_OVERWRITE, flags))
		cdb_flags = CDB_PUT_INSERT;
	else
		cdb_flags = CDB_PUT_REPLACE;

	r = cdb_make_put(&sm_cdbmap->cdbmap_map.cdbs_cdb_wr,
			key->data, key->size, data->data, data->size,
			cdb_flags);
	if (r > 0)
	{
		if (bitset(SMDBF_NO_OVERWRITE, flags))
			return SMDBE_DUPLICATE;
		else
			return SMDBE_OK;
	}
	return r;
}


static int
smcdb_set_owner(database, uid, gid)
	SMDB_DATABASE *database;
	uid_t uid;
	gid_t gid;
{
# if HASFCHOWN
	int fd;
	int result;
	SMDB_CDB_DATABASE *sm_cdbmap = (SMDB_CDB_DATABASE *) database->smdb_impl;

	assert(sm_cdbmap != NULL);
	fd = sm_cdbmap->cdbmap_fd;
	if (fd >= 0)
	{
		result = fchown(fd, uid, gid);
		if (result < 0)
			return errno;
	}
# endif /* HASFCHOWN */

	return SMDBE_OK;
}

static int
smcdb_sync(database, flags)
	SMDB_DATABASE *database;
	unsigned int flags;
{
	return 0;
}

static int
smcdb_cursor_close(cursor)
	SMDB_CURSOR *cursor;
{
	int ret;

	ret = SMDBE_OK;
	if (cursor != NULL)
		free(cursor);
	return ret;
}

static int
smcdb_cursor_del(cursor, flags)
	SMDB_CURSOR *cursor;
	SMDB_FLAG flags;
{
	return -1;
}

static int
smcdb_cursor_get(cursor, key, value, flags)
	SMDB_CURSOR *cursor;
	SMDB_DBENT *key;
	SMDB_DBENT *value;
	SMDB_FLAG flags;
{
	SMDB_CDB_DATABASE *sm_cdbmap;
	size_t l;
	int ret;

	ret = SMDBE_OK;
	sm_cdbmap = cursor->smdbc_impl;
	ret = cdb_seqnext(&sm_cdbmap->smcdb_pos, &sm_cdbmap->cdbmap_map.cdbs_cdb_rd);
	if (ret == 0)
		return SMDBE_LAST_ENTRY;
	if (ret < 0)
		return SMDBE_IO_ERROR;

	l = cdb_keylen(&sm_cdbmap->cdbmap_map.cdbs_cdb_rd);
	DBEALLOC(key, l);

	ret = cdb_read(&sm_cdbmap->cdbmap_map.cdbs_cdb_rd,
			key->data, l,
			cdb_keypos(&sm_cdbmap->cdbmap_map.cdbs_cdb_rd));
	if (ret < 0)
		return SMDBE_IO_ERROR;
	key->size = l;

	l = cdb_datalen(&sm_cdbmap->cdbmap_map.cdbs_cdb_rd);

	DBEALLOC(value, l);
	ret = cdb_read(&sm_cdbmap->cdbmap_map.cdbs_cdb_rd,
			value->data, l,
			cdb_datapos(&sm_cdbmap->cdbmap_map.cdbs_cdb_rd));
	if (ret < 0)
		return SMDBE_IO_ERROR;
	value->size = l;

	return SMDBE_OK;
}

static int
smcdb_cursor_put(cursor, key, value, flags)
	SMDB_CURSOR *cursor;
	SMDB_DBENT *key;
	SMDB_DBENT *value;
	SMDB_FLAG flags;
{
	return -1;
}

static int
smcdb_cursor(database, cursor, flags)
	SMDB_DATABASE *database;
	SMDB_CURSOR **cursor;
	SMDB_FLAG flags;
{
	int result;
	SMDB_CDB_DATABASE *sm_cdbmap;

	result = SMDBE_OK;
	*cursor = (SMDB_CURSOR *) malloc(sizeof(SMDB_CURSOR));
	if (*cursor == NULL)
		return SMDBE_MALLOC;

	sm_cdbmap = (SMDB_CDB_DATABASE *) database->smdb_impl;
	(*cursor)->smdbc_close = smcdb_cursor_close;
	(*cursor)->smdbc_del = smcdb_cursor_del;
	(*cursor)->smdbc_get = smcdb_cursor_get;
	(*cursor)->smdbc_put = smcdb_cursor_put;
	(*cursor)->smdbc_impl = sm_cdbmap;

	cdb_seqinit(&sm_cdbmap->smcdb_pos, &sm_cdbmap->cdbmap_map.cdbs_cdb_rd);

	return result;
}

/*
**  SMDB_CDB_OPEN -- Opens a cdb database.
**
**	Parameters:
**		database -- An unallocated database pointer to a pointer.
**		db_name -- The name of the database without extension.
**		mode -- File permissions for a created database.
**		mode_mask -- Mode bits that must match on an opened database.
**		sff -- Flags for safefile.
**		type -- The type of database to open
**			See smdb_type_to_cdb_type for valid types.
**		user_info -- User information for file permissions.
**		db_params -- unused
**
**	Returns:
**		SMDBE_OK -- Success, other errno:
**		SMDBE_MALLOC -- Cannot allocate memory.
**		SMDBE_BAD_OPEN -- various (OS) errors.
**		Anything else: translated error from cdb
*/

int
smdb_cdb_open(database, db_name, mode, mode_mask, sff, type, user_info, db_params)
	SMDB_DATABASE **database;
	char *db_name;
	int mode;
	int mode_mask;
	long sff;
	SMDB_DBTYPE type;
	SMDB_USER_INFO *user_info;
	SMDB_DBPARAMS *db_params;
{
	bool lockcreated = false;
	int result;
	int lock_fd;
	int db_fd;
	SMDB_DATABASE *smdb_db;
	SMDB_CDB_DATABASE *sm_cdbmap;
	struct stat stat_info;
	char db_file_name[MAXPATHLEN];

	*database = NULL;
	result = smdb_add_extension(db_file_name, sizeof db_file_name,
				    db_name, SMCDB_FILE_EXTENSION);
	if (result != SMDBE_OK)
		return result;

	result = smdb_setup_file(db_name, SMCDB_FILE_EXTENSION,
				 mode_mask, sff, user_info, &stat_info);
	if (result != SMDBE_OK)
		return result;

	lock_fd = -1;

	if (stat_info.st_mode == ST_MODE_NOFILE &&
	    bitset(mode, O_CREAT))
		lockcreated = true;

	result = smdb_lock_file(&lock_fd, db_name, mode, sff,
				SMCDB_FILE_EXTENSION);
	if (result != SMDBE_OK)
		return result;

	if (lockcreated)
	{
		mode |= O_TRUNC;
		mode &= ~(O_CREAT|O_EXCL);
	}

	smdb_db = smdb_malloc_database();
	sm_cdbmap = smcdb_malloc_database();
	if (sm_cdbmap == NULL || smdb_db == NULL)
	{
		smdb_unlock_file(lock_fd);
		smdb_free_database(smdb_db);	/* ok to be NULL */
		if (sm_cdbmap != NULL)
			free(sm_cdbmap);
		return SMDBE_MALLOC;
	}

	sm_cdbmap->smcdb_lock_fd = lock_fd;

# if 0
	db = NULL;
	db_flags = 0;
	if (bitset(O_CREAT, mode))
		db_flags |= DB_CREATE;
	if (bitset(O_TRUNC, mode))
		db_flags |= DB_TRUNCATE;
	if (mode == O_RDONLY)
		db_flags |= DB_RDONLY;
	SM_DB_FLAG_ADD(db_flags);
# endif

	result = -1; /* smdb_db_open_internal(db_file_name, db_type, db_flags, db_params, &db); */
	db_fd = open(db_file_name, mode, DBMMODE);
	if (db_fd == -1)
	{
		result = SMDBE_BAD_OPEN;
		goto error;
	}

	sm_cdbmap->cdbmap_create = (mode != O_RDONLY);
	if (mode == O_RDONLY)
		result = cdb_init(&sm_cdbmap->cdbmap_map.cdbs_cdb_rd, db_fd);
	else
		result = cdb_make_start(&sm_cdbmap->cdbmap_map.cdbs_cdb_wr, db_fd);
	if (result != 0)
	{
		result = SMDBE_BAD_OPEN;
		goto error;
	}

	if (result == 0)
		result = SMDBE_OK;
	else
	{
		/* Try and narrow down on the problem */
		if (result != 0)
			result = cdb_error_to_smdb(result);
		else
			result = SMDBE_BAD_OPEN;
	}

	if (result == SMDBE_OK)
		result = smdb_filechanged(db_name, SMCDB_FILE_EXTENSION, db_fd,
					  &stat_info);

	if (result == SMDBE_OK)
	{
		/* Everything is ok. Setup driver */
		/* smdb_db->smcdb_db = sm_cdbmap; */

		smdb_db->smdb_close = smcdb_close;
		smdb_db->smdb_del = smcdb_del;
		smdb_db->smdb_fd = smcdb_fd;
		smdb_db->smdb_lockfd = smcdb_lockfd;
		smdb_db->smdb_get = smcdb_get;
		smdb_db->smdb_put = smcdb_put;
		smdb_db->smdb_set_owner = smcdb_set_owner;
		smdb_db->smdb_sync = smcdb_sync;
		smdb_db->smdb_cursor = smcdb_cursor;
		smdb_db->smdb_impl = sm_cdbmap;

		*database = smdb_db;

		return SMDBE_OK;
	}

  error:
	if (sm_cdbmap != NULL)
	{
		/* close */
	}

	smdb_unlock_file(sm_cdbmap->smcdb_lock_fd);
	free(sm_cdbmap);
	smdb_free_database(smdb_db);

	return result;
}

#endif /* CDB */
