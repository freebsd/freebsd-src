#ifndef CATALOG_H
#define CATALOG_H 1

enum catalog_decl_type {
  CATALOG_NO_DECL = -1,
  CATALOG_ENTITY_DECL,
  CATALOG_DOCTYPE_DECL,
  CATALOG_LINKTYPE_DECL
};

#define CATALOG_SYSTEM_ERROR 1

#ifdef __STDC__

typedef void *CATALOG;
typedef void (*CATALOG_ERROR_HANDLER)(const char *filename,
				      unsigned long lineno,
				      int error_number,
				      unsigned flags,
				      int sys_errno);
CATALOG catalog_create(CATALOG_ERROR_HANDLER);
void catalog_load_file(CATALOG, const char *);
void catalog_delete(CATALOG);
int catalog_lookup_entity(CATALOG,
			  const char *public_id,
			  const char *name,
			  enum catalog_decl_type,
			  const char *subst_table,
			  const char **system_id,
			  const char **catalog_file);
const char *catalog_error_text(int error_number);

#else /* not __STDC__ */

typedef char *CATALOG;
typedef void (*CATALOG_ERROR_HANDLER)();
CATALOG catalog_create();
void catalog_load_file();
void catalog_delete();
int catalog_lookup_entity();
char *catalog_error_text();

#endif /* not __STDC__ */

#endif /* not CATALOG_H */
