/*
 * Mobile App Launcher Framework - Implementation
 * Scans .desktop files per Desktop Entry Specification
 * Manages app metadata and launches via zygote
 */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <fnmatch.h>
#include <pwd.h>
#include <time.h>

#include "app_launcher.h"
#include "../zygote/zygote.h"
#include "../ipc/ipc.h"

/* Static application registry */
static app_info_t g_apps[AL_MAX_APPS];
static int        g_app_count = 0;
static int        g_initialized = 0;

/* Permission mapping from category name to bitmask */
static struct {
    const char *cat_name;
    app_perms_t perms;
} g_category_perms[] = {
    { "AudioVideo",         PERM_STORAGE | PERM_INTERNET },
    { "Audio",              PERM_MICROPHONE | PERM_STORAGE },
    { "Video",              PERM_STORAGE | PERM_INTERNET },
    { "Development",        PERM_SYSTEM | PERM_STORAGE },
    { "Education",          PERM_STORAGE },
    { "Game",               PERM_INTERNET | PERM_STORAGE },
    { "Graphics",           PERM_STORAGE },
    { "Network",            PERM_INTERNET | PERM_BLUETOOTH },
    { "Office",             PERM_STORAGE },
    { "Science",            PERM_STORAGE },
    { "Settings",           PERM_SYSTEM },
    { "System",             PERM_SYSTEM | PERM_PHONE },
    { "Utility",            PERM_STORAGE },
    { "Phone",              PERM_PHONE | PERM_CONTACTS | PERM_SMS | PERM_STORAGE },
    { "Accessibility",      PERM_ACCESSIBILITY | PERM_SYSTEM },
    { "ContactManagement",  PERM_CONTACTS | PERM_PHONE },
    { "Calendar",           PERM_CONTACTS | PERM_STORAGE },
    { "Messaging",          PERM_SMS | PERM_CONTACTS | PERM_STORAGE },
    { "Maps",               PERM_LOCATION | PERM_INTERNET | PERM_STORAGE },
    { "Browser",            PERM_INTERNET | PERM_STORAGE },
    { "Photography",        PERM_CAMERA | PERM_STORAGE },
    { "Finance",            PERM_STORAGE | PERM_INTERNET },
    { "Health",             PERM_CONTACTS | PERM_STORAGE },
    { NULL,                 0 },
};

static app_perms_t
al_perms_for_category(const char *category)
{
    for (int i = 0; g_category_perms[i].cat_name; i++) {
        if (strcmp(g_category_perms[i].cat_name, category) == 0)
            return g_category_perms[i].perms;
    }
    return PERM_NONE;
}

static void
al_parse_permissions(const char *value, app_perms_t *out_perms)
{
    *out_perms = PERM_NONE;
    if (!value || !*value)
        return;

    char buf[256];
    strlcpy(buf, value, sizeof(buf));

    char *tok = strtok(buf, ";");
    while (tok) {
        while (isspace((unsigned char)*tok)) tok++;
        char *end = tok + strlen(tok) - 1;
        while (end > tok && isspace((unsigned char)*end)) end--;
        end[1] = '\0';

        if (strcmp(tok, "camera")         == 0) *out_perms |= PERM_CAMERA;
        else if (strcmp(tok, "microphone") == 0) *out_perms |= PERM_MICROPHONE;
        else if (strcmp(tok, "location")   == 0) *out_perms |= PERM_LOCATION;
        else if (strcmp(tok, "contacts")   == 0) *out_perms |= PERM_CONTACTS;
        else if (strcmp(tok, "sms")        == 0) *out_perms |= PERM_SMS;
        else if (strcmp(tok, "storage")    == 0) *out_perms |= PERM_STORAGE;
        else if (strcmp(tok, "internet")   == 0) *out_perms |= PERM_INTERNET;
        else if (strcmp(tok, "bluetooth")  == 0) *out_perms |= PERM_BLUETOOTH;
        else if (strcmp(tok, "nfc")        == 0) *out_perms |= PERM_NFC;
        else if (strcmp(tok, "phone")      == 0) *out_perms |= PERM_PHONE;
        else if (strcmp(tok, "system")     == 0) *out_perms |= PERM_SYSTEM;
        else if (strcmp(tok, "accessibility") == 0) *out_perms |= PERM_ACCESSIBILITY;

        tok = strtok(NULL, ";");
    }
}

static void
al_parse_categories(const char *value, app_info_t *info)
{
    if (!value || !*value)
        return;
    char buf[512];
    strlcpy(buf, value, sizeof(buf));

    char *tok = strtok(buf, ";");
    while (tok && info->cat_count < AL_CAT_COUNT_MAX) {
        while (isspace((unsigned char)*tok)) tok++;
        char *end = tok + strlen(tok) - 1;
        while (end > tok && isspace((unsigned char)*end)) end--;
        end[1] = '\0';
        if (*tok) {
            strlcpy(info->categories[info->cat_count], tok, AL_CAT_MAX);
            info->cat_count++;
        }
        tok = strtok(NULL, ";");
    }
}

static void
al_parse_mime_types(const char *value, app_info_t *info)
{
    if (!value || !*value)
        return;
    strlcpy(info->mime_types, value, sizeof(info->mime_types));
}

static void
al_strlcpy_clean(char *dst, size_t dstsz, const char *src)
{
    if (!src) { dst[0] = '\0'; return; }
    while (isspace((unsigned char)*src)) src++;
    strlcpy(dst, src, dstsz);
    char *end = dst + strlen(dst) - 1;
    while (end > dst && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
}

static int
al_load_desktop_file(const char *path, app_info_t *out_info)
{
    FILE *fp;
    char line[1024];
    int in_desktop_section = 0;

    if (!path || !out_info)
        return -1;

    fp = fopen(path, "r");
    if (!fp)
        return -1;

    memset(out_info, 0, sizeof(*out_info));
    out_info->type = APP_TYPE_APPLICATION;
    out_info->permissions = PERM_NONE;

    while (fgets(line, sizeof(line), fp)) {
        /* Remove newline */
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
            line[--len] = '\0';

        /* Skip empty and comment lines */
        char *p = line;
        while (isspace((unsigned char)*p)) p++;
        if (*p == '\0' || *p == '#')
            continue;

        /* Check section header */
        if (*p == '[') {
            char *end = strchr(p + 1, ']');
            if (end) {
                *end = '\0';
                if (strcasecmp(p + 1, "Desktop Entry") == 0)
                    in_desktop_section = 1;
                else
                    in_desktop_section = 0;
            }
            continue;
        }

        if (!in_desktop_section)
            continue;

        /* Parse key=value */
        char *eq = strchr(p, '=');
        if (!eq)
            continue;
        *eq = '\0';
        char *key = p;
        char *val = eq + 1;
        while (isspace((unsigned char)*key)) key++;
        while (isspace((unsigned char)*val)) val++;

        if (strcasecmp(key, "Type") == 0) {
            if (strcasecmp(val, "Link") == 0)
                out_info->type = APP_TYPE_LINK;
            else if (strcasecmp(val, "Directory") == 0)
                out_info->type = APP_TYPE_DIRECTORY;
        } else if (strcasecmp(key, "Name") == 0) {
            al_strlcpy_clean(out_info->name, sizeof(out_info->name), val);
        } else if (strcasecmp(key, "Exec") == 0) {
            al_strlcpy_clean(out_info->exec, sizeof(out_info->exec), val);
        } else if (strcasecmp(key, "Icon") == 0) {
            al_strlcpy_clean(out_info->icon, sizeof(out_info->icon), val);
        } else if (strcasecmp(key, "Categories") == 0) {
            al_parse_categories(val, out_info);
        } else if (strcasecmp(key, "MimeType") == 0) {
            al_parse_mime_types(val, out_info);
        } else if (strcasecmp(key, "Permissions") == 0) {
            al_parse_permissions(val, &out_info->permissions);
        } else if (strcasecmp(key, "TryExec") == 0) {
            /* Optional: try exec validation */
        }
    }

    fclose(fp);

    /* Generate ID from filename if not derivable from .desktop */
    const char *fname = strrchr(path, '/');
    if (fname)
        fname++;
    else
        fname = path;
    snprintf(out_info->id, sizeof(out_info->id), "%s", fname);
    char *dot = strrchr(out_info->id, '.');
    if (dot) *dot = '\0';

    /* Validate: application type needs Exec */
    if (out_info->type == APP_TYPE_APPLICATION && out_info->exec[0] == '\0')
        return -1;

    return 0;
}

static int
al_scan_directory(const char *dirpath)
{
    DIR *dir;
    struct dirent *ent;

    dir = opendir(dirpath);
    if (!dir)
        return -1;

    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_type != DT_REG && ent->d_type != DT_LNK)
            continue;

        size_t namelen = strlen(ent->d_name);
        if (namelen <= strlen(AL_DESKTOP_EXT))
            continue;
        if (strcasecmp(ent->d_name + namelen - strlen(AL_DESKTOP_EXT),
                       AL_DESKTOP_EXT) != 0)
            continue;

        char fullpath[AL_PATH_MAX];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dirpath, ent->d_name);

        app_info_t info;
        if (al_load_desktop_file(fullpath, &info) != 0)
            continue;

        /* Apply category-based default permissions */
        for (int i = 0; i < info.cat_count; i++) {
            info.permissions |= al_perms_for_category(info.categories[i]);
        }

        if (g_app_count < AL_MAX_APPS) {
            g_apps[g_app_count] = info;
            g_app_count++;
        }
    }

    closedir(dir);
    return 0;
}

static int
al_fuzzy_match(const char *haystack, const char *needle)
{
    if (!needle || !*needle)
        return 1;
    if (!haystack || !*haystack)
        return 0;

    /* Case-insensitive contains */
    const char *h = haystack;
    const char *n = needle;
    while (*h) {
        const char *hh = h;
        const char *nn = n;
        while (*hh && *nn && tolower((unsigned char)*hh) == tolower((unsigned char)*nn)) {
            hh++; nn++;
        }
        if (!*nn)
            return 1;
        h++;
    }
    return 0;
}

int
al_init(void)
{
    if (g_initialized)
        return 0;

    g_app_count = 0;

    /* Scan primary application directory */
    al_scan_directory(AL_APP_DIR);

    /* Also scan user applications */
    const char *user_dirs[] = {
        "/home/.local/share/applications",
        "/mobile/share/applications/user",
        NULL
    };

    for (int i = 0; user_dirs[i]; i++) {
        al_scan_directory(user_dirs[i]);
    }

    g_initialized = 1;
    return 0;
}

void
al_shutdown(void)
{
    g_app_count = 0;
    memset(g_apps, 0, sizeof(g_apps));
    g_initialized = 0;
}

int
al_get_all_apps(app_info_t **out_apps, int *out_count)
{
    if (!out_apps || !out_count)
        return -1;

    if (g_app_count == 0 && !g_initialized)
        al_init();

    *out_apps = g_apps;
    *out_count = g_app_count;
    return 0;
}

app_info_t *
al_get_app_by_id(const char *app_id)
{
    if (!app_id)
        return NULL;

    if (g_app_count == 0 && !g_initialized)
        al_init();

    for (int i = 0; i < g_app_count; i++) {
        if (strcmp(g_apps[i].id, app_id) == 0)
            return &g_apps[i];
    }
    return NULL;
}

int
al_get_category(const char *category, app_info_t **out_apps, int *out_count)
{
    if (!category || !out_apps || !out_count)
        return -1;

    static app_info_t results[AL_MAX_APPS];
    int count = 0;

    if (g_app_count == 0 && !g_initialized)
        al_init();

    for (int i = 0; i < g_app_count && count < AL_MAX_APPS; i++) {
        for (int c = 0; c < g_apps[i].cat_count; c++) {
            if (strcmp(g_apps[i].categories[c], category) == 0) {
                results[count++] = g_apps[i];
                break;
            }
        }
    }

    *out_apps = results;
    *out_count = count;
    return 0;
}

int
al_filter(const al_filter_query_t *query, app_info_t **out_apps, int *out_count)
{
    if (!query || !out_apps || !out_count)
        return -1;

    static app_info_t results[AL_MAX_APPS];
    int count = 0;

    if (g_app_count == 0 && !g_initialized)
        al_init();

    for (int i = 0; i < g_app_count && count < AL_MAX_APPS; i++) {
        /* Category filter */
        if (query->category && query->category[0]) {
            int cat_match = 0;
            for (int c = 0; c < g_apps[i].cat_count; c++) {
                if (strcmp(g_apps[i].categories[c], query->category) == 0) {
                    cat_match = 1;
                    break;
                }
            }
            if (!cat_match)
                continue;
        }

        /* Query text filter (fuzzy match on name, id, categories) */
        if (query->query && query->query[0]) {
            int match = 0;
            if (al_fuzzy_match(g_apps[i].name, query->query))
                match = 1;
            else if (al_fuzzy_match(g_apps[i].id, query->query))
                match = 1;
            else {
                for (int c = 0; c < g_apps[i].cat_count; c++) {
                    if (al_fuzzy_match(g_apps[i].categories[c], query->query)) {
                        match = 1;
                        break;
                    }
                }
            }
            if (!match)
                continue;
        }

        /* Permission filter: app must have all required permissions */
        if (query->required_perms && (g_apps[i].permissions & query->required_perms) != query->required_perms)
            continue;

        results[count++] = g_apps[i];
    }

    *out_apps = results;
    *out_count = count;
    return 0;
}

int
al_launch(const char *app_id)
{
    app_info_t *app = al_get_app_by_id(app_id);
    if (!app) {
        return -1;
    }

    pid_t pid = zygote_fork(app->id, app->permissions);
    if (pid < 0) {
        return -1;
    }

    app->launched_count++;
    return 0;
}

int
al_get_app_count(void)
{
    if (g_app_count == 0 && !g_initialized)
        al_init();
    return g_app_count;
}

void
al_free_apps(app_info_t *apps)
{
    (void)apps;
}
