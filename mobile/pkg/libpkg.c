#include "libpkg.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#define PKGLIST_PATH "/var/db/pkgctl/pkglist.txt"
#define REPO_CONF_PATH "/etc/pkgctl/repos.conf"
#define DB_DIR "/var/db/pkgctl"
#define ETCDIR "/etc/pkgctl"

struct pkg_db {
    int dummy; /* opaque */
};

static struct pkg_db *db_handle = NULL;

/* Ensure directory exists */
static int
ensure_dir(const char *path)
{
    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode))
        return 0;
    if (mkdir(path, 0755) == 0)
        return 0;
    if (errno != EEXIST) {
        perror("mkdir");
        return -1;
    }
    return 0;
}

/* Open the package database */
struct pkg_db *
pkg_db_open(void)
{
    if (db_handle != NULL)
        return db_handle;

    /* Ensure directories exist */
    if (ensure_dir(DB_DIR) != 0 || ensure_dir(ETCDIR) != 0)
        return NULL;

    db_handle = calloc(1, sizeof(struct pkg_db));
    if (db_handle == NULL) {
        perror("calloc");
        return NULL;
    }
    return db_handle;
}

/* Close the package database */
void
pkg_db_close(struct pkg_db *db)
{
    if (db == NULL)
        return;
    if (db != db_handle) {
        fprintf(stderr, "pkg_db_close: attempted to close unknown handle\n");
        return;
    }
    free(db_handle);
    db_handle = NULL;
}

/* Helper: check if database is open */
static int
db_check(void)
{
    if (db_handle == NULL) {
        fprintf(stderr, "pkgctl: database not opened\n");
        return -1;
    }
    return 0;
}

/* Install a package by name */
int
pkg_install(const char *name)
{
    if (db_check() != 0)
        return -1;

    /* For now, just simulate installation by adding to pkglist.txt */
    int fd = open(PKGLIST_PATH, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd == -1) {
        perror("open pkglist.txt");
        return -1;
    }

    char buf[256];
    int len = snprintf(buf, sizeof(buf), "%s:1.0:description\n", name);
    if (write(fd, buf, len) != len) {
        perror("write");
        close(fd);
        return -1;
    }
    close(fd);
    printf("Installed %s\n", name);
    return 0;
}

/* Remove an installed package by name */
int
pkg_remove(const char *name)
{
    if (db_check() != 0)
        return -1;

    /* Read the entire file, filter out the package, and rewrite */
    int fd_in = open(PKGLIST_PATH, O_RDONLY, 0);
    if (fd_in == -1) {
        if (errno == ENOENT) {
            fprintf(stderr, "Package %s not installed\n", name);
            return 0;
        }
        perror("open pkglist.txt for reading");
        return -1;
    }

    char tmp[] = "/var/db/pkgctl/pkglist.txt.XXXXXX";
    int fd_out = mkstemp(tmp);
    if (fd_out == -1) {
        perror("mkstemp");
        close(fd_in);
        return -1;
    }

    char buf[1024];
    ssize_t n;
    int found = 0;
    while ((n = read(fd_in, buf, sizeof(buf))) > 0) {
        char *start = buf;
        char *end;
        while ((end = memchr(start, '\n', buf + n - start)) != NULL) {
            size_t linelen = end - start;
            char line[256];
            if (linelen >= sizeof(line))
                linelen = sizeof(line) - 1;
            memcpy(line, start, linelen);
            line[linelen] = '\0';
            /* Check if line starts with name: */
            if (strncmp(line, name, strlen(name)) == 0 && line[strlen(name)] == ':') {
                found = 1;
                /* Skip this line */
            } else {
                /* Write line plus newline */
                if (write(fd_out, line, linelen) != linelen ||
                    write(fd_out, "\n", 1) != 1) {
                    perror("write");
                    close(fd_in);
                    close(fd_out);
                    unlink(tmp);
                    return -1;
                }
            }
            start = end + 1;
        }
        /* Handle partial line (should not happen with line-buffered input) */
        if (start < buf + n) {
            /* For simplicity, we assume lines are complete */
            /* In reality, we'd need to handle partial lines */
            /* We'll just copy the partial line */
            if (write(fd_out, start, buf + n - start) != (buf + n - start)) {
                perror("write");
                close(fd_in);
                close(fd_out);
                unlink(tmp);
                return -1;
            }
        }
    }
    if (n == -1) {
        perror("read");
        close(fd_in);
        close(fd_out);
        unlink(tmp);
        return -1;
    }

    close(fd_in);
    if (close(fd_out) == -1) {
        perror("close");
        unlink(tmp);
        return -1;
    }

    if (!found) {
        fprintf(stderr, "Package %s not found in installed list\n", name);
        unlink(tmp);
        return 0;
    }

    if (rename(tmp, PKGLIST_PATH) == -1) {
        perror("rename");
        unlink(tmp);
        return -1;
    }

    printf("Removed %s\n", name);
    return 0;
}

/* Search for packages by name */
int
pkg_search(const char *name)
{
    if (db_check() != 0)
        return -1;

    int fd = open(PKGLIST_PATH, O_RDONLY, 0);
    if (fd == -1) {
        if (errno == ENOENT) {
            printf("No packages installed\n");
            return 0;
        }
        perror("open pkglist.txt");
        return -1;
    }

    char buf[1024];
    ssize_t n;
    int found = 0;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        char *start = buf;
        char *end;
        while ((end = memchr(start, '\n', buf + n - start)) != NULL) {
            size_t linelen = end - start;
            if (linelen > 0 && buf[linelen-1] == '\r')
                linelen--;
            char line[256];
            if (linelen >= sizeof(line))
                linelen = sizeof(line) - 1;
            memcpy(line, start, linelen);
            line[linelen] = '\0';
            if (strstr(line, name) != NULL) {
                printf("%s\n", line);
                found = 1;
            }
            start = end + 1;
        }
        /* Handle partial line */
        if (start < buf + n) {
            /* For simplicity, we assume lines are complete */
            /* We'll just ignore partial line at end */
        }
    }
    if (n == -1) {
        perror("read");
        close(fd);
        return -1;
    }
    close(fd);

    if (!found)
        printf("No packages matching '%s'\n", name);
    return 0;
}

/* Show information about an installed package */
int
pkg_info(const char *name)
{
    if (db_check() != 0)
        return -1;

    int fd = open(PKGLIST_PATH, O_RDONLY, 0);
    if (fd == -1) {
        if (errno == ENOENT) {
            fprintf(stderr, "No packages installed\n");
            return 0;
        }
        perror("open pkglist.txt");
        return -1;
    }

    char buf[1024];
    ssize_t n;
    int found = 0;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        char *start = buf;
        char *end;
        while ((end = memchr(start, '\n', buf + n - start)) != NULL) {
            size_t linelen = end - start;
            if (linelen > 0 && buf[linelen-1] == '\r')
                linelen--;
            char line[256];
            if (linelen >= sizeof(line))
                linelen = sizeof(line) - 1;
            memcpy(line, start, linelen);
            line[linelen] = '\0';
            /* Format: name:version:desc */
            if (strncmp(line, name, strlen(name)) == 0 && line[strlen(name)] == ':') {
                printf("%s\n", line);
                found = 1;
                break;
            }
            start = end + 1;
        }
        if (found)
            break;
        /* Handle partial line */
        if (start < buf + n) {
            /* For simplicity, we assume lines are complete */
            /* We'll just ignore partial line at end */
        }
    }
    if (n == -1) {
        perror("read");
        close(fd);
        return -1;
    }
    close(fd);

    if (!found) {
        fprintf(stderr, "Package %s not installed\n", name);
        return 0;
    }
    return 0;
}

/* Add a repository URL to the configuration */
int
pkg_repo_add(const char *url)
{
    if (db_check() != 0)
        return -1;

    /* Basic URL check: must start with http:// or https:// or ftp:// */
    if (strncmp(url, "http://", 7) != 0 &&
        strncmp(url, "https://", 8) != 0 &&
        strncmp(url, "ftp://", 6) != 0) {
        fprintf(stderr, "Invalid repository URL: must start with http://, https://, or ftp://\n");
        return -1;
    }

    int fd = open(REPO_CONF_PATH, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd == -1) {
        perror("open repos.conf");
        return -1;
    }

    char buf[256];
    int len = snprintf(buf, sizeof(buf), "%s\n", url);
    if (write(fd, buf, len) != len) {
        perror("write");
        close(fd);
        return -1;
    }
    close(fd);
    printf("Added repository: %s\n", url);
    return 0;
}

/* Update package indexes from repositories */
int
pkg_repo_update(void)
{
    if (db_check() != 0)
        return -1;

    /* Mock: just check that the repos.conf exists and write a timestamp */
    int fd = open(REPO_CONF_PATH, O_RDONLY, 0);
    if (fd == -1) {
        if (errno == ENOENT) {
            fprintf(stderr, "No repositories configured\n");
            return 0;
        }
        perror("open repos.conf");
        return -1;
    }
    close(fd);

    /* Create a timestamp file in /var/db/pkgctl/ */
    char timestamp_path[256];
    snprintf(timestamp_path, sizeof(timestamp_path), "/var/db/pkgctl/repo_timestamp");

    fd = open(timestamp_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        perror("open timestamp file");
        return -1;
    }

    char buf[32];
    time_t now = time(NULL);
    int len = snprintf(buf, sizeof(buf), "%ld\n", (long)now);
    if (write(fd, buf, len) != len) {
        perror("write timestamp");
        close(fd);
        return -1;
    }
    close(fd);

    printf("Updated package indexes\n");
    return 0;
}

/* Check if upgrades are available for installed packages */
int
pkg_upgrade_check(void)
{
    if (db_check() != 0)
        return -1;

    /* Mock: always report no upgrades */
    printf("No upgrades available\n");
    return 0;
}

/* Update the local package database (alias for repo update) */
int
pkg_update(void)
{
    return pkg_repo_update();
}

/* Upgrade all installed packages */
int
pkg_upgrade(void)
{
    if (db_check() != 0)
        return -1;

    /* Mock: do nothing */
    printf("Upgraded all packages\n");
    return 0;
}

/* Query package (alias for info) */
int
pkg_query(const char *name)
{
    return pkg_info(name);
}