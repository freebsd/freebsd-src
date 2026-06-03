/*
 * Resource Management - Implementation
 * Display metrics, config, density, strings, asset loading
 */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <sys/queue.h>

#include "resource.h"

#define RES_STRINGS_PATH "/mobile/share/strings.xml"
#define RES_ASSET_DIR    "/mobile/share/assets"
#define RES_MAX_STRINGS  4096

static int g_res_init = 0;
static display_t g_display = {
    720, 1280, RES_DPI_HDPI, 60,
    62,  110
};
static config_t  g_config = {
    RES_ORIENTATION_PORTRAIT, RES_UI_MODE_NORMAL, RES_TOUCHSCREEN_FINGER, 600, 1280, 720
};

typedef struct string_entry {
    int    id;
    char  *value;
    SLIST_ENTRY(string_entry) link;
} string_entry_t;

static SLIST_HEAD(str_head, string_entry) g_string_table;
static pthread_mutex_t g_str_lock = PTHREAD_MUTEX_INITIALIZER;

static void
res_parse_strings_xml(const char *path)
{
    if (!path) return;

    int fd = open(path, O_RDONLY);
    if (fd < 0) return;

    char *buf = malloc(65536);
    if (!buf) { close(fd); return; }

    ssize_t n = read(fd, buf, 65535);
    close(fd);
    if (n <= 0) { free(buf); return; }
    buf[n] = '\0';

    const char *p = buf;
    while ((p = strstr(p, "<string name=\"")) != NULL) {
        p += 14;
        char name[256] = "";
        int ni = 0;
        while (*p && *p != '"' && ni < 255) name[ni++] = *p++;
        if (*p != '"') break;
        p++;

        if (strncmp(p, ">", 1) != 0) continue;
        p++;

        char value[2048] = "";
        int vi = 0;
        int in_entity = 0;
        while (*p && !(p[0] == '<' && p[1] == '/')) {
            if (p[0] == '&' && p[1] == '#' && p[2] == 'x') {
                long cp = strtol(p + 3, NULL, 16);
                if (cp > 0 && cp < 0x110000) value[vi++] = (char)cp;
                p = strchr(p, ';');
                if (p) p++; else break;
            } else {
                value[vi++] = *p++;
            }
        }
        value[vi] = '\0';

        string_entry_t *se = malloc(sizeof(*se));
        if (se) {
            se->id = (int)strtoul(name, NULL, 10);
            se->value = strdup(value);
            pthread_mutex_lock(&g_str_lock);
            SLIST_INSERT_HEAD(&g_string_table, se, link);
            pthread_mutex_unlock(&g_str_lock);
        }
    }
    free(buf);
}

int
res_init(void)
{
    if (g_res_init)
        return 0;

    SLIST_INIT(&g_string_table);
    res_parse_strings_xml(RES_STRINGS_PATH);
    g_res_init = 1;
    return 0;
}

void
res_shutdown(void)
{
    pthread_mutex_lock(&g_str_lock);
    string_entry_t *se;
    while ((se = SLIST_FIRST(&g_string_table)) != NULL) {
        SLIST_REMOVE_HEAD(&g_string_table, link);
        free(se->value);
        free(se);
    }
    pthread_mutex_unlock(&g_str_lock);
    g_res_init = 0;
}

int
res_get_display(display_t *out_disp)
{
    if (!out_disp) return -1;
    *out_disp = g_display;
    return 0;
}

int
res_get_config(config_t *out_cfg)
{
    if (!out_cfg) return -1;
    *out_cfg = g_config;
    return 0;
}

int
res_density_to_dpi(const char *density)
{
    if (!density) return RES_DPI_MDPI;

    if (strcmp(density, "ldpi")   == 0) return RES_DPI_LDPI;
    if (strcmp(density, "mdpi")   == 0) return RES_DPI_MDPI;
    if (strcmp(density, "hdpi")   == 0) return RES_DPI_HDPI;
    if (strcmp(density, "xhdpi")  == 0) return RES_DPI_XHDPI;
    if (strcmp(density, "xxhdpi") == 0) return RES_DPI_XXHDPI;
    if (strcmp(density, "xxxhdpi")== 0) return RES_DPI_XXXHDPI;

    return atoi(density);
}

float
res_scale_factor(void)
{
    return (float)g_display.density_dpi / (float)RES_DPI_MDPI;
}

int
res_get_string(int id, char *out_buf, size_t max_len)
{
    if (!out_buf || max_len == 0) return -1;

    pthread_mutex_lock(&g_str_lock);
    string_entry_t *se;
    SLIST_FOREACH(se, &g_string_table, link) {
        if (se->id == id) {
            strlcpy(out_buf, se->value, max_len);
            pthread_mutex_unlock(&g_str_lock);
            return 0;
        }
    }
    pthread_mutex_unlock(&g_str_lock);

    snprintf(out_buf, max_len, "string_%d", id);
    return -1;
}

asset_t *
res_load_asset(const char *type, const char *name)
{
    if (!type || !name) return NULL;

    char path[512];
    snprintf(path, sizeof(path), "%s/%s/%s", RES_ASSET_DIR, type, name);

    int fd = open(path, O_RDONLY);
    if (fd < 0) return NULL;

    struct stat st;
    if (fstat(fd, &st) < 0) { close(fd); return NULL; }

    asset_t *asset = malloc(sizeof(*asset));
    if (!asset) { close(fd); return NULL; }

    asset->data = malloc(st.st_size);
    if (!asset->data) { free(asset); close(fd); return NULL; }

    ssize_t n = read(fd, asset->data, st.st_size);
    close(fd);
    if (n < 0) { free(asset->data); free(asset); return NULL; }

    asset->size = (size_t)n;
    strlcpy(asset->type, type, sizeof(asset->type));
    return asset;
}

void
res_free_asset(asset_t *asset)
{
    if (!asset) return;
    if (asset->data) free(asset->data);
    free(asset);
}

int
res_dpi_to_density(int dpi)
{
    if (dpi < RES_DPI_MDPI / 2)
        return RES_DPI_LDPI;
    if (dpi < RES_DPI_MDPI * 1.5)
        return RES_DPI_MDPI;
    if (dpi < RES_DPI_HDPI * 1.5)
        return RES_DPI_HDPI;
    if (dpi < RES_DPI_XHDPI * 1.5)
        return RES_DPI_XHDPI;
    if (dpi < RES_DPI_XXHDPI * 1.5)
        return RES_DPI_XXHDPI;
    return RES_DPI_XXXHDPI;
}
