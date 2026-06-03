/*
 * Application Launcher Implementation
 */

#include "app_launcher.h"
#include "../services/service_mgr.h"
#include <stdlib.h>
#include <string.h>

static launcher_t *g_launcher = NULL;

static void trim(char *s) {
    char *end = s + strlen(s);
    while (end > s && (*end == ' ' || *end == '\n' || *end == '\r' || *end == '\t')) end--;
    *end = 0;
}

static char *get_value(const char *data, const char *key) {
    static char buf[256];
    char search[64];
    snprintf(search, sizeof(search), "%s=", key);
    const char *p = strstr(data, search);
    if (!p) return NULL;
    p += strlen(search);
    int i = 0;
    while (*p && *p != '\n' && i < 255) buf[i++] = *p++;
    buf[i] = 0;
    trim(buf);
    return buf;
}

static int is_bool(const char *val) {
    return strcmp(val, "true") == 0 || strcmp(val, "1") == 0;
}

static int parse_categories(const char *val, char out[][LAUNCHER_CAT_MAX], int max) {
    int count = 0;
    const char *start = val;
    while (*start && count < max) {
        const char *semicolon = strchr(start, ';');
        int len = semicolon ? (int)(semicolon - start) : (int)strlen(start);
        if (len > 0 && len < LAUNCHER_CAT_MAX - 1) {
            strncpy(out[count], start, len);
            out[count][len] = 0;
            trim(out[count]);
            count++;
        }
        if (!semicolon) break;
        start = semicolon + 1;
    }
    return count;
}

int launcher_init(const char *apps_dir) {
    if (g_launcher) return 0;
    g_launcher = calloc(1, sizeof(launcher_t));
    if (!g_launcher) return -1;
    strncpy(g_launcher->apps_dir, apps_dir ? apps_dir : "/usr/share/applications", sizeof(g_launcher->apps_dir) - 1);
    g_launcher->count = 0;
    return 0;
}

void launcher_shutdown(void) {
    if (!g_launcher) return;
    free(g_launcher);
    g_launcher = NULL;
}

int launcher_scan_directory(const char *path) {
    (void)path;
    return 0;
}

int launcher_parse_desktop_file(const char *path, desktop_app_t *app) {
    if (!path || !app) return -1;
    char data[4096];
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    size_t rd = fread(data, 1, sizeof(data) - 1, f);
    fclose(f);
    data[rd] = 0;
    memset(app, 0, sizeof(*app));
    char *name = get_value(data, "Name");
    if (name) strncpy(app->name, name, sizeof(app->name) - 1);
    char *exec = get_value(data, "Exec");
    if (exec) strncpy(app->exec, exec, sizeof(app->exec) - 1);
    char *icon = get_value(data, "Icon");
    if (icon) strncpy(app->icon, icon, sizeof(app->icon) - 1);
    char *cats = get_value(data, "Categories");
    if (cats) app->category_count = parse_categories(cats, app->categories, LAUNCHER_MAX_CATEGORIES);
    char *mime = get_value(data, "MimeType");
    if (mime) {
        const char *start = mime;
        int mc = 0;
        while (*start && mc < LAUNCHER_MIME_MAX) {
            const char *semi = strchr(start, ';');
            int len = semi ? (int)(semi - start) : (int)strlen(start);
            if (len > 0 && len < 63) {
                strncpy(app->mime_types[mc], start, len);
                app->mime_types[mc][len] = 0;
                mc++;
            }
            if (!semi) break;
            start = semi + 1;
        }
        app->mime_count = mc;
    }
    app->terminal = is_bool(get_value(data, "Terminal") ? get_value(data, "Terminal") : "false");
    app->hidden = is_bool(get_value(data, "NoDisplay") ? get_value(data, "NoDisplay") : "false");
    if (path) strncpy(app->path, path, sizeof(app->path) - 1);
    const char *fname = strrchr(path, '/');
    if (fname) {
        char tmp[128];
        strncpy(tmp, fname + 1, sizeof(tmp) - 1);
        char *dot = strrchr(tmp, '.');
        if (dot) *dot = 0;
        strncpy(app->id, tmp, sizeof(app->id) - 1);
    }
    return 0;
}

char **launcher_get_categories(int *count) {
    static char *cats[32] = {0};
    if (count) *count = 0;
    return cats;
}

desktop_app_t *launcher_search(const char *query) {
    if (!g_launcher || !query || !query[0]) return NULL;
    int best_idx = -1;
    float best_score = 0.0f;
    for (int i = 0; i < g_launcher->count; i++) {
        float score = 0.0f;
        if (strcmp(g_launcher->apps[i].name, query) == 0) score = 100.0f;
        else if (strstr(g_launcher->apps[i].name, query)) score = 70.0f;
        else {
            for (int c = 0; c < g_launcher->apps[i].category_count && score < 30.0f; c++)
                if (strstr(g_launcher->apps[i].categories[c], query)) score = 30.0f;
        }
        if (score > best_score) { best_score = score; best_idx = i; }
    }
    return best_idx >= 0 ? &g_launcher->apps[best_idx] : NULL;
}

desktop_app_t *launcher_get_by_id(const char *id) {
    if (!g_launcher || !id) return NULL;
    for (int i = 0; i < g_launcher->count; i++)
        if (strcmp(g_launcher->apps[i].id, id) == 0) return &g_launcher->apps[i];
    return NULL;
}

int launcher_launch(const char *app_id) {
    desktop_app_t *app = launcher_get_by_id(app_id);
    return app ? launcher_launch_app(app) : -1;
}

int launcher_launch_app(desktop_app_t *app) {
    if (!app || !app->exec[0]) return -1;
    return svc_start(app->exec);
}

void launcher_refresh(void) {
    if (!g_launcher) return;
    g_launcher->count = 0;
}

launcher_t *launcher_get_instance(void) { return g_launcher; }
