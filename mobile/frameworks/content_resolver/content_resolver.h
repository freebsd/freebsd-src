/*
 * Content Resolver - Unified data access API
 * URI-based: content://contacts/people, content://media/images
 */

#ifndef _CONTENT_RESOLVER_H_
#define _CONTENT_RESOLVER_H_

#include <sys/types.h>
#include <stdint.h>
#include <stdlib.h>

#define CR_URI_MAX       512
#define CR_MAX_PROVIDERS 64
#define CR_MAX_COLUMNS   32
#define CR_MAX_RESULT    65536
#define CR_MAX_MIME      64

typedef enum {
    CURSOR_TYPE_STRING  = 1,
    CURSOR_TYPE_INT     = 2,
    CURSOR_TYPE_LONG    = 3,
    CURSOR_TYPE_FLOAT   = 4,
    CURSOR_TYPE_BLOB    = 5,
} cursor_type_t;

typedef struct cursor {
    int         col_count;
    char        col_names[CR_MAX_COLUMNS][64];
    cursor_type_t col_types[CR_MAX_COLUMNS];
    void       *row_data;
    int         row_count;
    int         pos;
} cursor_t;

typedef struct content_provider {
    char     authority[128];
    char     mime_types[CR_MAX_MIME];
    int      (*query)(const char *uri, const char **projection,
                      const char *selection, const char **selection_args,
                      const char *sort_order, cursor_t *out);
    int      (*insert)(const char *uri, const void *values, uint32_t len);
    int      (*update)(const char *uri, const void *values, uint32_t len,
                       const char *selection, const char *sel_args);
    int      (*delete)(const char *uri, const char *selection, const char *sel_args);
    char     mime_type[128];
} content_provider_t;

typedef struct content_resolver {
    content_provider_t providers[CR_MAX_PROVIDERS];
    int                provider_count;
} content_resolver_t;

int         cr_init(void);
void        cr_shutdown(void);
int         cr_register_provider(const char *authority, const content_provider_t *provider);
cursor_t   *cr_query(const char *uri, const char **projection,
                     const char *selection, const char **sel_args, const char *sort);
int         cr_insert(const char *uri, const void *values, uint32_t len);
int         cr_update(const char *uri, const void *values, uint32_t len,
                     const char *selection, const char *sel_args);
int         cr_delete(const char *uri, const char *selection, const char *sel_args);

int         cr_get_type(const char *uri, char *out_mime, size_t max_len);

int         cursor_get_count(cursor_t *cur);
int         cursor_get_int(cursor_t *cur, int col, int *out_val);
int         cursor_get_long(cursor_t *cur, int col, int64_t *out_val);
int         cursor_get_string(cursor_t *cur, int col, char *out_buf, size_t max_len);
int         cursor_get_blob(cursor_t *cur, int col, void **out_data, size_t *out_len);
void        cursor_destroy(cursor_t *cur);

int         cr_grant_uri_permission(const char *to_pkg, const char *uri, int mode);

#endif /* _CONTENT_RESOLVER_H_ */
