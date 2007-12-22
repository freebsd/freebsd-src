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
extern const char method_names[][16];	/* change this in root.c if you change
					   the enum above */

typedef struct cvsroot_s {
    char *original;		/* The complete source CVSroot string. */
    CVSmethod method;		/* One of the enum values above. */
    char *directory;		/* The directory name. */
#ifdef CLIENT_SUPPORT
    char *username;		/* The username or NULL if method == local. */
    char *password;		/* The password or NULL if method == local. */
    char *hostname;		/* The hostname or NULL if method == local. */
    int port;			/* The port or zero if method == local. */
    char *proxy_hostname;	/* The hostname of the proxy server, or NULL
				 * when method == local or no proxy will be
				 * used.
				 */
    int proxy_port;		/* The port of the proxy or zero, as above. */
    unsigned char isremote;	/* Nonzero if we are doing remote access. */
#endif /* CLIENT_SUPPORT */
} cvsroot_t;
