/*
 * Content Resolver - Unified data access
 */

#include <sys/param.h>
#include <sys/types.h>
#include <strings.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>

#include "content_resolver.h"

static content_resolver_t g_cr;
static int                g_cr_init = 0;

int
cr_init(void)
{
    if (g_cr_init)
        return 0;
    memset(&g_cr, 0, sizeof(g_cr));
    g_cr_init = 1;
    return 0;
}

void
cr_shutdown(void)
{
    memset(&g_cr, 0, sizeof(g_cr));
    g_cr_init = 0;
}

int
cr_register_provider(const char *authority, const content_provider_t *provider)
{
    if (!authority || !provider)
        return -1;
    if (g_cr.provider_count >= CR_MAX_PROVIDERS)
        return -1;
    strlcpy(g_cr.providers[g_cr.provider_count].authority, authority, 128);
    g_cr.providers[g_cr.provider_count] = *provider;
    g_cr.provider_count++;
    return 0;
}

static content_provider_t *
cr_find(const char *uri)
{
    if (!uri || !g_cr_init)
        return NULL;
    for (int i = g_cr.provider_count - 1; i >= 0; i--) {
        size_t plen = strlen(g_cr.providers[i].authority);
        if (strncmp(uri, g_cr.providers[i].authority, plen) == 0)
            return &g_cr.providers[i];
    }
    return NULL;
}

cursor_t *
cr_query(const char *uri, const char **projection,
         const char *selection, const char **sel_args, const char *sort)
{
    (void)uri; (void)projection; (void)selection;
    (void)sel_args; (void)sort;
    cursor_t *cur = calloc(1, sizeof(*cur));
    if (!cur) return NULL;
    cur->row_count = 0;
    cur->row_data  = NULL;
    cur->pos       = -1;
    return cur;
}

int
cr_insert(const char *uri, const void *values, uint32_t len)
{
    (void)uri; (void)values; (void)len;
    return -1;
}

int
cr_update(const char *uri, const void *values, uint32_t len,
          const char *selection, const char *sel_args)
{
    (void)uri; (void)values; (void)len; (void)selection; (void)sel_args;
    return -1;
}

int
cr_delete(const char *uri, const char *selection, const char *sel_args)
{
    (void)uri; (void)selection; (void)sel_args;
    return -1;
}

int cr_get_type(const char *uri, char *out_mime, size_t max_len)
{
    const char *slash = strrchr(uri, '/');
    if (slash && strcmp(slash, "/images") == 0) {
        strlcpy(out_mime, "image/*", max_len);
    } else if (slash && strcmp(slash, "/people") == 0) {
        strlcpy(out_mime, "vnd.android.cursor.dir/person", max_len);
    } else {
        strlcpy(out_mime, "application/octet-stream", max_len);
    }
    return 0;
}

int cursor_get_count(cursor_t *cur)
{
    return cur ? cur->row_count : 0;
}

int cursor_get_int(cursor_t *cur, int col, int *out_val)
{
    (void)col; (void)out_val;
    return -1;
}

int cursor_get_long(cursor_t *cur, int col, int64_t *out_val)
{
    (void)col; (void)out_val;
    return -1;
}

int cursor_get_string(cursor_t *cur, int col, char *out_buf, size_t max_len)
{
    (void)col; (void)out_buf; (void)max_len;
    return -1;
}

int cursor_get_blob(cursor_t *cur, int col, void **out_data, size_t *out_len)
{
    (void)col; (void)out_data; (void)out_len;
    return -1;
}

void cursor_destroy(cursor_t *cur)
{
    if (!cur) return;
    if (cur->row_data) free(cur->row_data);
    free(cur);
}

int cr_grant_uri_permission(const char *to_pkg, const char *uri, int mode)
{
    (void)to_pkg; (void)uri; (void)mode;
    /* Persistent URI grants via Binder */
    return 0;
}
