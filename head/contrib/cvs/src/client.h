/*
 * Copyright (C) 1994-2008 The Free Software Foundation, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/* Interface between the client and the rest of CVS.  */

/* Stuff shared with the server.  */
extern char *mode_to_string PROTO((mode_t));
extern int change_mode PROTO((char *, char *, int));

extern int gzip_level;
extern int file_gzip_level;

#if defined (CLIENT_SUPPORT) || defined (SERVER_SUPPORT)

/* Whether the connection should be encrypted.  */
extern int cvsencrypt;

/* Whether the connection should use per-packet authentication.  */
extern int cvsauthenticate;

#ifdef __STDC__
struct buffer;
#endif

# ifdef ENCRYPTION

#   ifdef HAVE_KERBEROS

/* We can't declare the arguments without including krb.h, and I don't
   want to do that in every file.  */
extern struct buffer *krb_encrypt_buffer_initialize ();

#   endif /* HAVE_KERBEROS */

#   ifdef HAVE_GSSAPI

/* Set this to turn on GSSAPI encryption.  */
extern int cvs_gssapi_encrypt;

#   endif /* HAVE_GSSAPI */

# endif /* ENCRYPTION */

# ifdef HAVE_GSSAPI

/* We can't declare the arguments without including gssapi.h, and I
   don't want to do that in every file.  */
extern struct buffer *cvs_gssapi_wrap_buffer_initialize ();

# endif /* HAVE_GSSAPI */

#endif /* defined (CLIENT_SUPPORT) || defined (SERVER_SUPPORT) */

#ifdef CLIENT_SUPPORT
/*
 * Flag variable for seeing whether the server has been started yet.
 * As of this writing, only edit.c:cvs_notify_check() uses it.
 */
extern int server_started;

/* Is the -P option to checkout or update specified?  */
extern int client_prune_dirs;

# ifdef AUTH_CLIENT_SUPPORT
extern int use_authenticating_server;
# endif /* AUTH_CLIENT_SUPPORT */
# if defined (AUTH_CLIENT_SUPPORT) || defined (HAVE_GSSAPI)
void connect_to_pserver PROTO ((cvsroot_t *,
				struct buffer **,
				struct buffer **,
				int, int ));
#   ifndef CVS_AUTH_PORT
#     define CVS_AUTH_PORT 2401
#   endif /* CVS_AUTH_PORT */
# endif /* (AUTH_CLIENT_SUPPORT) || defined (HAVE_GSSAPI) */

# if HAVE_KERBEROS
#   ifndef CVS_PORT
#     define CVS_PORT 1999
#   endif
# endif /* HAVE_KERBEROS */

/* Talking to the server. */
void send_to_server PROTO((const char *str, size_t len));
void read_from_server PROTO((char *buf, size_t len));

/* Internal functions that handle client communication to server, etc.  */
int supported_request PROTO ((char *));
void option_with_arg PROTO((char *option, char *arg));

/* Get the responses and then close the connection.  */
extern int get_responses_and_close PROTO((void));

extern int get_server_responses PROTO((void));

/* Start up the connection to the server on the other end.  */
void
start_server PROTO((void));

/* Send the names of all the argument files to the server.  */
void
send_file_names PROTO((int argc, char **argv, unsigned int flags));

/* Flags for send_file_names.  */
/* Expand wild cards?  */
# define SEND_EXPAND_WILD 1

/*
 * Send Repository, Modified and Entry.  argc and argv contain only
 * the files to operate on (or empty for everything), not options.
 * local is nonzero if we should not recurse (-l option).
 */
void
send_files PROTO((int argc, char **argv, int local, int aflag,
		  unsigned int flags));

/* Flags for send_files.  */
# define SEND_BUILD_DIRS 1
# define SEND_FORCE 2
# define SEND_NO_CONTENTS 4
# define BACKUP_MODIFIED_FILES 8

/* Send an argument to the remote server.  */
void
send_arg PROTO((const char *string));

/* Send a string of single-char options to the remote server, one by one.  */
void send_options PROTO ((int argc, char * const *argv));

extern void send_a_repository PROTO ((const char *, const char *,
                                      const char *));

#endif /* CLIENT_SUPPORT */

/*
 * This structure is used to catalog the responses the client is
 * prepared to see from the server.
 */

struct response
{
    /* Name of the response.  */
    char *name;

#ifdef CLIENT_SUPPORT
    /*
     * Function to carry out the response.  ARGS is the text of the
     * command after name and, if present, a single space, have been
     * stripped off.  The function can scribble into ARGS if it wants.
     * Note that although LEN is given, ARGS is also guaranteed to be
     * '\0' terminated.
     */
    void (*func) PROTO((char *args, int len));

    /*
     * ok and error are special; they indicate we are at the end of the
     * responses, and error indicates we should exit with nonzero
     * exitstatus.
     */
    enum {response_type_normal, response_type_ok, response_type_error} type;
#endif

    /* Used by the server to indicate whether response is supported by
       the client, as set by the Valid-responses request.  */
    enum {
      /*
       * Failure to implement this response can imply a fatal
       * error.  This should be set only for responses which were in the
       * original version of the protocol; it should not be set for new
       * responses.
       */
      rs_essential,

      /* Some clients might not understand this response.  */
      rs_optional,

      /*
       * Set by the server to one of the following based on what this
       * client actually supports.
       */
      rs_supported,
      rs_not_supported
      } status;
};

/* Table of responses ending in an entry with a NULL name.  */

extern struct response responses[];

#ifdef CLIENT_SUPPORT

extern void client_senddate PROTO((const char *date));
extern void client_expand_modules PROTO((int argc, char **argv, int local));
extern void client_send_expansions PROTO((int local, char *where,
					  int build_dirs));
extern void client_nonexpanded_setup PROTO((void));

extern void send_init_command PROTO ((void));

extern char **failed_patches;
extern int failed_patches_count;
extern char *toplevel_wd;
extern void client_import_setup PROTO((char *repository));
extern int client_process_import_file
    PROTO((char *message, char *vfile, char *vtag,
	   int targc, char *targv[], char *repository, int all_files_binary,
	   int modtime));
extern void client_import_done PROTO((void));
extern void client_notify PROTO((const char *, const char *, const char *, int,
                                 const char *));
#endif /* CLIENT_SUPPORT */
