/*
 * Copyright (c) 2001, Derek Price and others
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 *
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS kit.
 */

/* CVSroot data structures */

/* Access method specified in CVSroot. */
typedef enum {
    null_method,
    local_method,
    server_method,
    pserver_method,
    kserver_method,
    gserver_method,
    ext_method,
    fork_method
} CVSmethod;
extern char *method_names[];	/* change this in root.c if you change
				   the enum above */

typedef struct cvsroot_s {
    char *original;		/* the complete source CVSroot string */
    CVSmethod method;		/* one of the enum values above */
    char *username;		/* the username or NULL if method == local */
    char *password;		/* the username or NULL if method == local */
    char *hostname;		/* the hostname or NULL if method == local */
    int port;			/* the port or zero if method == local */
    char *directory;		/* the directory name */
#ifdef CLIENT_SUPPORT
    unsigned char isremote;	/* nonzero if we are doing remote access */
#endif /* CLIENT_SUPPORT */
} cvsroot_t;
