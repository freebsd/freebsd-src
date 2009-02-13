/*
 * Copyright (C) 1986-2005 The Free Software Foundation, Inc.
 *
 * Portions Copyright (C) 1998-2005 Derek Price, Ximbiot <http://ximbiot.com>,
 *                                  and others.
 *
 * Portions Copyright (C) 1992, Brian Berliner and Jeff Polk
 * Portions Copyright (C) 1989-1992, Brian Berliner
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
    extssh_method,
    fork_method
} CVSmethod;
extern const char method_names[][16];	/* change this in root.c if you change
					   the enum above */

typedef struct cvsroot_s {
    char *original;		/* The complete source CVSroot string. */
    CVSmethod method;		/* One of the enum values above. */
    char *directory;		/* The directory name. */
    unsigned char isremote;	/* Nonzero if we are doing remote access. */
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
#endif /* CLIENT_SUPPORT */
} cvsroot_t;

cvsroot_t *Name_Root PROTO((const char *dir, const char *update_dir));
void free_cvsroot_t PROTO((cvsroot_t *root_in));
cvsroot_t *parse_cvsroot PROTO((const char *root));
cvsroot_t *local_cvsroot PROTO((const char *dir));
void Create_Root PROTO((const char *dir, const char *rootdir));
void root_allow_add PROTO ((char *));
void root_allow_free PROTO ((void));
int root_allow_ok PROTO ((char *));
int root_allow_used PROTO ((void));
