/*
 * prof-int.h
 */

#include "k5-platform.h"
#include "k5-thread.h"
#include "k5-plugin.h"

#include <time.h>

#if defined(__MACH__) && defined(__APPLE__)
#include <TargetConditionals.h>
#define PROFILE_SUPPORTS_FOREIGN_NEWLINES
#endif

#include "com_err.h"
#include "profile.h"

typedef long prf_magic_t;

/*
 * This is the structure which stores the profile information for a
 * particular configuration file.
 *
 * Locking strategy:
 * - filespec, fslen are fixed after creation
 * - refcount and next should only be tweaked with the global lock held
 * - other fields can be tweaked after grabbing the in-struct lock
 */
struct _prf_data_t {
	prf_magic_t	magic;
	k5_mutex_t	lock;
	struct profile_node *root;
	time_t		last_stat;
	time_t		timestamp; /* time tree was last updated from file */
	unsigned long	frac_ts;   /* fractional part of timestamp, if any */
	int		flags;	/* r/w, dirty */
	int		upd_serial; /* incremented when data changes */

	size_t		fslen;

	/* Some separation between fields controlled by different
	   mutexes.  Theoretically, both could be accessed at the same
	   time from different threads on different CPUs with separate
	   caches.  Don't let the threads clobber each other's
	   changes.  One mutex controlling the whole thing would be
	   better, but sufficient separation might suffice.

	   This is icky.  I just hope it's adequate.

	   For next major release, fix this.  */
	union { double d; void *p; uint64_t ll; k5_mutex_t m; } pad;

	int		refcount; /* prf_file_t references */
	struct _prf_data_t *next;
	/* Was: "profile_filespec_t filespec".  Now: flexible char
	   array ... except, we need to work in C89, so an array
	   length must be specified.  */
	const char	filespec[sizeof("/etc/krb5.conf")];
};

typedef struct _prf_data_t *prf_data_t;
prf_data_t profile_make_prf_data(const char *);

struct _prf_file_t {
	prf_magic_t	magic;
	struct _prf_data_t	*data;
	struct _prf_file_t *next;
};

typedef struct _prf_file_t *prf_file_t;

/*
 * The profile flags
 *
 * Deprecated use of read/write profile flag.
 * Check whether file is writable lazily so we don't call access as often.
 */
#define PROFILE_FILE_DEPRECATED_RW	0x0001
#define PROFILE_FILE_DIRTY		0x0002
#define PROFILE_FILE_SHARED		0x0004

struct _prf_lib_handle_t {
	k5_mutex_t lock;
	int refcount;
	struct plugin_file_handle *plugin_handle;
};

typedef struct _prf_lib_handle_t *prf_lib_handle_t;

/*
 * This structure defines the high-level, user visible profile_t
 * object, which is used as a handle by users who need to query some
 * configuration file(s)
 */
struct _profile_t {
	prf_magic_t	magic;
	prf_file_t	first_file;

	/* If non-null, use vtable operations instead of native ones. */
	struct profile_vtable *vt;
	void		*cbdata;
	prf_lib_handle_t lib_handle;
};

/*
 * Used by the profile iterator in prof_get.c
 */
#define PROFILE_ITER_LIST_SECTION	0x0001
#define PROFILE_ITER_SECTIONS_ONLY	0x0002
#define PROFILE_ITER_RELATIONS_ONLY	0x0004

#define PROFILE_ITER_FINAL_SEEN		0x0100

/*
 * Check if a filespec is last in a list (NULL on UNIX, invalid FSSpec on MacOS
 */

#define	PROFILE_LAST_FILESPEC(x) (((x) == NULL) || ((x)[0] == '\0'))

/* profile_parse.c */

errcode_t profile_parse_file
	(FILE *f, struct profile_node **root, char **ret_modspec);

errcode_t profile_process_directory
	(const char *dirname, struct profile_node **root);

errcode_t profile_write_tree_file
	(struct profile_node *root, FILE *dstfile);

errcode_t profile_write_tree_to_buffer
	(struct profile_node *root, char **buf);


/* prof_tree.c */

void profile_free_node
	(struct profile_node *relation);

errcode_t profile_create_node
	(const char *name, const char *value,
		   struct profile_node **ret_node);

errcode_t profile_verify_node
	(struct profile_node *node);

errcode_t profile_add_node
	(struct profile_node *section,
		    const char *name, const char *value,
		    struct profile_node **ret_node);

errcode_t profile_make_node_final
	(struct profile_node *node);

int profile_is_node_final
	(struct profile_node *node);

const char *profile_get_node_name
	(struct profile_node *node);

const char *profile_get_node_value
	(struct profile_node *node);

errcode_t profile_find_node
	(struct profile_node *section,
		    const char *name, const char *value,
		    int section_flag, void **state,
		    struct profile_node **node);

errcode_t profile_find_node_relation
	(struct profile_node *section,
		    const char *name, void **state,
		    char **ret_name, char **value);

errcode_t profile_find_node_subsection
	(struct profile_node *section,
		    const char *name, void **state,
		    char **ret_name, struct profile_node **subsection);

errcode_t profile_get_node_parent
	(struct profile_node *section,
		   struct profile_node **parent);

errcode_t profile_delete_node_relation
	(struct profile_node *section, const char *name);

errcode_t profile_find_node_name
	(struct profile_node *section, void **state,
		    char **ret_name);

errcode_t profile_node_iterator_create
	(profile_t profile, const char *const *names,
		   int flags, void **ret_iter);

void profile_node_iterator_free
	(void	**iter_p);

errcode_t profile_node_iterator
	(void	**iter_p, struct profile_node **ret_node,
		   char **ret_name, char **ret_value);

errcode_t profile_remove_node
	(struct profile_node *node);

errcode_t profile_set_relation_value
	(struct profile_node *node, const char *new_value);

errcode_t profile_rename_node
	(struct profile_node *node, const char *new_name);

/* prof_file.c */

errcode_t KRB5_CALLCONV profile_copy (profile_t, profile_t *);

errcode_t profile_open_file
	(const_profile_filespec_t file, prf_file_t *ret_prof,
	 char **ret_modspec);

#define profile_update_file(P, M) profile_update_file_data((P)->data, M)
errcode_t profile_update_file_data
	(prf_data_t profile, char **ret_modspec);
#define profile_update_file_locked(P, M) profile_update_file_data_locked((P)->data, M)
errcode_t profile_update_file_data_locked
	(prf_data_t data, char **ret_modspec);

#define profile_flush_file(P) (((P) && (P)->magic == PROF_MAGIC_FILE) ? profile_flush_file_data((P)->data) : PROF_MAGIC_FILE)
errcode_t profile_flush_file_data
	(prf_data_t data);

#define profile_flush_file_to_file(P,F) (((P) && (P)->magic == PROF_MAGIC_FILE) ? profile_flush_file_data_to_file((P)->data, (F)) : PROF_MAGIC_FILE)
errcode_t profile_flush_file_data_to_file
	(prf_data_t data, const char *outfile);

errcode_t profile_flush_file_data_to_buffer
	(prf_data_t data, char **bufp);

void profile_free_file
	(prf_file_t profile);

errcode_t profile_close_file
	(prf_file_t profile);

int profile_file_is_writable
	(prf_file_t profile);

void profile_dereference_data (prf_data_t);
void profile_dereference_data_locked (prf_data_t);

void profile_lock_global (void);
void profile_unlock_global (void);

/* prof_init.c -- included from profile.h */
errcode_t profile_ser_size
        (const char *, profile_t, size_t *);

errcode_t profile_ser_externalize
        (const char *, profile_t, unsigned char **, size_t *);

errcode_t profile_ser_internalize
        (const char *, profile_t *, unsigned char **, size_t *);

/* prof_get.c */

errcode_t profile_get_value
	(profile_t profile, const char **names, char **ret_value);
/* Others included from profile.h */

/* prof_set.c -- included from profile.h */
