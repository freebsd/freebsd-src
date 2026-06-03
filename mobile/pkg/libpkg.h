#ifndef LIBPKG_H
#define LIBPKG_H

#include <stddef.h>

/* Opaque handle for the package database */
struct pkg_db;

/* Open the package database, return a handle or NULL on failure */
struct pkg_db *pkg_db_open(void);

/* Close the package database */
void pkg_db_close(struct pkg_db *db);

/* Install a package by name (fetches from repo and installs) */
int pkg_install(const char *name);

/* Remove an installed package by name */
int pkg_remove(const char *name);

/* Search for packages by name (prints matches) */
int pkg_search(const char *name);

/* Show information about an installed package */
int pkg_info(const char *name);

/* Add a repository URL to the configuration */
int pkg_repo_add(const char *url);

/* Update package indexes from repositories */
int pkg_repo_update(void);

/* Check if upgrades are available for installed packages */
int pkg_upgrade_check(void);

/* Update the local package database (alias for repo update?) */
int pkg_update(void);

/* Upgrade all installed packages */
int pkg_upgrade(void);

/* Query package (alias for info?) */
int pkg_query(const char *name);

#endif /* LIBPKG_H */