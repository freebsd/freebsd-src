/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright 1994 by OpenVision Technologies, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appears in all copies and
 * that both that copyright notice and this permission notice appear in
 * supporting documentation, and that the name of OpenVision not be used
 * in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission. OpenVision makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * OPENVISION DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL OPENVISION BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
 * USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
/*
 * Copyright (C) 2004,2005 by the Massachusetts Institute of Technology.
 * All rights reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#include <winsock.h>
#else
#include "port-sockets.h"
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdlib.h>
#include <ctype.h>

#include <gssapi/gssapi_generic.h>
#include <gssapi/gssapi_krb5.h>
#include "gss-misc.h"

#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

static OM_uint32
enumerateAttributes(OM_uint32 *minor, gss_name_t name, int noisy);
static OM_uint32
showLocalIdentity(OM_uint32 *minor, gss_name_t name);

static void
usage()
{
    fprintf(stderr, "Usage: gss-server [-port port] [-verbose] [-once]");
#ifdef _WIN32
    fprintf(stderr, " [-threads num]");
#endif
    fprintf(stderr, "\n");
    fprintf(stderr,
            "       [-inetd] [-export] [-logfile file] [-keytab keytab]\n"
            "       service_name\n");
    exit(1);
}

static FILE *logfile;

int     verbose = 0;

/*
 * Function: server_acquire_creds
 *
 * Purpose: imports a service name and acquires credentials for it
 *
 * Arguments:
 *
 *      service_name    (r) the ASCII service name
 *      mech            (r) the desired mechanism (or GSS_C_NO_OID)
 *      server_creds    (w) the GSS-API service credentials
 *
 * Returns: 0 on success, -1 on failure
 *
 * Effects:
 *
 * The service name is imported with gss_import_name, and service
 * credentials are acquired with gss_acquire_cred.  If either operation
 * fails, an error message is displayed and -1 is returned; otherwise,
 * 0 is returned.  If mech is given, credentials are acquired for the
 * specified mechanism.
 */

static int
server_acquire_creds(char *service_name, gss_OID mech,
                     gss_cred_id_t *server_creds)
{
    gss_buffer_desc name_buf;
    gss_name_t server_name;
    OM_uint32 maj_stat, min_stat;
    gss_OID_set_desc mechlist;
    gss_OID_set mechs = GSS_C_NO_OID_SET;

    name_buf.value = service_name;
    name_buf.length = strlen(name_buf.value) + 1;
    maj_stat = gss_import_name(&min_stat, &name_buf,
                               (gss_OID) gss_nt_service_name, &server_name);
    if (maj_stat != GSS_S_COMPLETE) {
        display_status("importing name", maj_stat, min_stat);
        return -1;
    }

    if (mech != GSS_C_NO_OID) {
        mechlist.count = 1;
        mechlist.elements = mech;
        mechs = &mechlist;
    }
    maj_stat = gss_acquire_cred(&min_stat, server_name, 0, mechs, GSS_C_ACCEPT,
                                server_creds, NULL, NULL);
    if (maj_stat != GSS_S_COMPLETE) {
        display_status("acquiring credentials", maj_stat, min_stat);
        return -1;
    }

    (void) gss_release_name(&min_stat, &server_name);

    return 0;
}

/*
 * Function: server_establish_context
 *
 * Purpose: establishses a GSS-API context as a specified service with
 * an incoming client, and returns the context handle and associated
 * client name
 *
 * Arguments:
 *
 *      s               (r) an established TCP connection to the client
 *      service_creds   (r) server credentials, from gss_acquire_cred
 *      context         (w) the established GSS-API context
 *      client_name     (w) the client's ASCII name
 *
 * Returns: 0 on success, -1 on failure
 *
 * Effects:
 *
 * Any valid client request is accepted.  If a context is established,
 * its handle is returned in context and the client name is returned
 * in client_name and 0 is returned.  If unsuccessful, an error
 * message is displayed and -1 is returned.
 */
static int
server_establish_context(int s, gss_cred_id_t server_creds,
                         gss_ctx_id_t *context, gss_buffer_t client_name,
                         OM_uint32 *ret_flags)
{
    gss_buffer_desc send_tok, recv_tok;
    gss_name_t client;
    gss_OID doid;
    OM_uint32 maj_stat, min_stat, acc_sec_min_stat;
    gss_buffer_desc oid_name;
    int     token_flags;

    if (recv_token(s, &token_flags, &recv_tok) < 0)
        return -1;

    if (recv_tok.value) {
        free(recv_tok.value);
        recv_tok.value = NULL;
    }

    if (!(token_flags & TOKEN_NOOP)) {
        if (logfile)
            fprintf(logfile, "Expected NOOP token, got %d token instead\n",
                    token_flags);
        return -1;
    }

    *context = GSS_C_NO_CONTEXT;

    if (token_flags & TOKEN_CONTEXT_NEXT) {
        do {
            if (recv_token(s, &token_flags, &recv_tok) < 0)
                return -1;

            if (verbose && logfile) {
                fprintf(logfile, "Received token (size=%d): \n",
                        (int) recv_tok.length);
                print_token(&recv_tok);
            }

            maj_stat = gss_accept_sec_context(&acc_sec_min_stat, context,
                                              server_creds, &recv_tok,
                                              GSS_C_NO_CHANNEL_BINDINGS,
                                              &client, &doid, &send_tok,
                                              ret_flags,
                                              NULL,  /* time_rec */
                                              NULL); /* del_cred_handle */

            if (recv_tok.value) {
                free(recv_tok.value);
                recv_tok.value = NULL;
            }

            if (send_tok.length != 0) {
                if (verbose && logfile) {
                    fprintf(logfile,
                            "Sending accept_sec_context token (size=%d):\n",
                            (int) send_tok.length);
                    print_token(&send_tok);
                }
                if (send_token(s, TOKEN_CONTEXT, &send_tok) < 0) {
                    if (logfile)
                        fprintf(logfile, "failure sending token\n");
                    return -1;
                }

                (void) gss_release_buffer(&min_stat, &send_tok);
            }
            if (maj_stat != GSS_S_COMPLETE
                && maj_stat != GSS_S_CONTINUE_NEEDED) {
                display_status("accepting context", maj_stat,
                               acc_sec_min_stat);
                if (*context != GSS_C_NO_CONTEXT)
                    gss_delete_sec_context(&min_stat, context,
                                           GSS_C_NO_BUFFER);
                return -1;
            }

            if (verbose && logfile) {
                if (maj_stat == GSS_S_CONTINUE_NEEDED)
                    fprintf(logfile, "continue needed...\n");
                else
                    fprintf(logfile, "\n");
                fflush(logfile);
            }
        } while (maj_stat == GSS_S_CONTINUE_NEEDED);

        /* display the flags */
        display_ctx_flags(*ret_flags);

        if (verbose && logfile) {
            maj_stat = gss_oid_to_str(&min_stat, doid, &oid_name);
            if (maj_stat != GSS_S_COMPLETE) {
                display_status("converting oid->string", maj_stat, min_stat);
                return -1;
            }
            fprintf(logfile, "Accepted connection using mechanism OID %.*s.\n",
                    (int) oid_name.length, (char *) oid_name.value);
            (void) gss_release_buffer(&min_stat, &oid_name);
        }

        maj_stat = gss_display_name(&min_stat, client, client_name, &doid);
        if (maj_stat != GSS_S_COMPLETE) {
            display_status("displaying name", maj_stat, min_stat);
            return -1;
        }
        enumerateAttributes(&min_stat, client, TRUE);
        showLocalIdentity(&min_stat, client);
        maj_stat = gss_release_name(&min_stat, &client);
        if (maj_stat != GSS_S_COMPLETE) {
            display_status("releasing name", maj_stat, min_stat);
            return -1;
        }
    } else {
        client_name->length = *ret_flags = 0;

        if (logfile)
            fprintf(logfile, "Accepted unauthenticated connection.\n");
    }

    return 0;
}

/*
 * Function: create_socket
 *
 * Purpose: Opens a listening TCP socket.
 *
 * Arguments:
 *
 *      port            (r) the port number on which to listen
 *
 * Returns: the listening socket file descriptor, or -1 on failure
 *
 * Effects:
 *
 * A listening socket on the specified port and created and returned.
 * On error, an error message is displayed and -1 is returned.
 */
static int
create_socket(u_short port)
{
    struct sockaddr_in saddr;
    int     s;
    int     on = 1;

    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(port);
    saddr.sin_addr.s_addr = INADDR_ANY;

    if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("creating socket");
        return -1;
    }
    /* Let the socket be reused right away */
    (void) setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *) &on, sizeof(on));
    if (bind(s, (struct sockaddr *) &saddr, sizeof(saddr)) < 0) {
        perror("binding socket");
        (void) closesocket(s);
        return -1;
    }
    if (listen(s, 5) < 0) {
        perror("listening on socket");
        (void) closesocket(s);
        return -1;
    }
    return s;
}

static float
timeval_subtract(struct timeval *tv1, struct timeval *tv2)
{
    return ((tv1->tv_sec - tv2->tv_sec) +
            ((float) (tv1->tv_usec - tv2->tv_usec)) / 1000000);
}

/*
 * Yes, yes, this isn't the best place for doing this test.
 * DO NOT REMOVE THIS UNTIL A BETTER TEST HAS BEEN WRITTEN, THOUGH.
 *                                      -TYT
 */
static int
test_import_export_context(gss_ctx_id_t *context)
{
    OM_uint32 min_stat, maj_stat;
    gss_buffer_desc context_token, copied_token;
    struct timeval tm1, tm2;

    /*
     * Attempt to save and then restore the context.
     */
    gettimeofday(&tm1, (struct timezone *) 0);
    maj_stat = gss_export_sec_context(&min_stat, context, &context_token);
    if (maj_stat != GSS_S_COMPLETE) {
        display_status("exporting context", maj_stat, min_stat);
        return 1;
    }
    gettimeofday(&tm2, (struct timezone *) 0);
    if (verbose && logfile)
        fprintf(logfile, "Exported context: %d bytes, %7.4f seconds\n",
                (int) context_token.length, timeval_subtract(&tm2, &tm1));
    copied_token.length = context_token.length;
    copied_token.value = malloc(context_token.length);
    if (copied_token.value == 0) {
        if (logfile)
            fprintf(logfile,
                    "Couldn't allocate memory to copy context token.\n");
        return 1;
    }
    memcpy(copied_token.value, context_token.value, copied_token.length);
    maj_stat = gss_import_sec_context(&min_stat, &copied_token, context);
    if (maj_stat != GSS_S_COMPLETE) {
        display_status("importing context", maj_stat, min_stat);
        return 1;
    }
    free(copied_token.value);
    gettimeofday(&tm1, (struct timezone *) 0);
    if (verbose && logfile)
        fprintf(logfile, "Importing context: %7.4f seconds\n",
                timeval_subtract(&tm1, &tm2));
    (void) gss_release_buffer(&min_stat, &context_token);
    return 0;
}

/*
 * Function: sign_server
 *
 * Purpose: Performs the "sign" service.
 *
 * Arguments:
 *
 *      s               (r) a TCP socket on which a connection has been
 *                      accept()ed
 *      service_name    (r) the ASCII name of the GSS-API service to
 *                      establish a context as
 *      export          (r) whether to test context exporting
 *
 * Returns: -1 on error
 *
 * Effects:
 *
 * sign_server establishes a context, and performs a single sign request.
 *
 * A sign request is a single GSS-API sealed token.  The token is
 * unsealed and a signature block, produced with gss_sign, is returned
 * to the sender.  The context is the destroyed and the connection
 * closed.
 *
 * If any error occurs, -1 is returned.
 */
static int
sign_server(int s, gss_cred_id_t server_creds, int export)
{
    gss_buffer_desc client_name, recv_buf, unwrap_buf, mic_buf, *msg_buf, *send_buf;
    gss_ctx_id_t context;
    OM_uint32 maj_stat, min_stat;
    int     i, conf_state;
    OM_uint32 ret_flags;
    char   *cp;
    int     token_flags;
    int     send_flags;

    /* Establish a context with the client */
    if (server_establish_context(s, server_creds, &context,
                                 &client_name, &ret_flags) < 0)
        return (-1);

    if (context == GSS_C_NO_CONTEXT) {
        printf("Accepted unauthenticated connection.\n");
    } else {
        printf("Accepted connection: \"%.*s\"\n",
               (int) client_name.length, (char *) client_name.value);
        (void) gss_release_buffer(&min_stat, &client_name);

        if (export) {
            for (i = 0; i < 3; i++)
                if (test_import_export_context(&context))
                    return -1;
        }
    }

    do {
        /* Receive the message token */
        if (recv_token(s, &token_flags, &recv_buf) < 0)
            return (-1);

        if (token_flags & TOKEN_NOOP) {
            if (logfile)
                fprintf(logfile, "NOOP token\n");
            if (recv_buf.value) {
                free(recv_buf.value);
                recv_buf.value = 0;
            }
            break;
        }

        if (verbose && logfile) {
            fprintf(logfile, "Message token (flags=%d):\n", token_flags);
            print_token(&recv_buf);
        }

        if ((context == GSS_C_NO_CONTEXT) &&
            (token_flags & (TOKEN_WRAPPED | TOKEN_ENCRYPTED | TOKEN_SEND_MIC)))
        {
            if (logfile)
                fprintf(logfile,
                        "Unauthenticated client requested authenticated services!\n");
            if (recv_buf.value) {
                free(recv_buf.value);
                recv_buf.value = 0;
            }
            return (-1);
        }

        if (token_flags & TOKEN_WRAPPED) {
            maj_stat = gss_unwrap(&min_stat, context, &recv_buf, &unwrap_buf,
                                  &conf_state, (gss_qop_t *) NULL);
            if (maj_stat != GSS_S_COMPLETE) {
                display_status("unsealing message", maj_stat, min_stat);
                if (recv_buf.value) {
                    free(recv_buf.value);
                    recv_buf.value = 0;
                }
                return (-1);
            } else if (!conf_state && (token_flags & TOKEN_ENCRYPTED)) {
                fprintf(stderr, "Warning!  Message not encrypted.\n");
            }

            if (recv_buf.value) {
                free(recv_buf.value);
                recv_buf.value = 0;
            }
            msg_buf = &unwrap_buf;
        } else {
            unwrap_buf.value = NULL;
            unwrap_buf.length = 0;
            msg_buf = &recv_buf;
        }

        if (logfile) {
            fprintf(logfile, "Received message: ");
            cp = msg_buf->value;
            if ((isprint((int) cp[0]) || isspace((int) cp[0])) &&
                (isprint((int) cp[1]) || isspace((int) cp[1]))) {
                fprintf(logfile, "\"%.*s\"\n", (int) msg_buf->length,
                        (char *) msg_buf->value);
            } else {
                fprintf(logfile, "\n");
                print_token(msg_buf);
            }
        }

        if (token_flags & TOKEN_SEND_MIC) {
            /* Produce a signature block for the message */
            maj_stat = gss_get_mic(&min_stat, context, GSS_C_QOP_DEFAULT,
                                   msg_buf, &mic_buf);
            if (maj_stat != GSS_S_COMPLETE) {
                display_status("signing message", maj_stat, min_stat);
                return (-1);
            }
            send_flags = TOKEN_MIC;
            send_buf = &mic_buf;
        } else {
            mic_buf.value = NULL;
            mic_buf.length = 0;
            send_flags = TOKEN_NOOP;
            send_buf = empty_token;
        }
        if (recv_buf.value) {
            free(recv_buf.value);
            recv_buf.value = NULL;
        }
        if (unwrap_buf.value) {
            gss_release_buffer(&min_stat, &unwrap_buf);
        }

        /* Send the signature block or NOOP to the client */
        if (send_token(s, send_flags, send_buf) < 0)
            return (-1);

        if (mic_buf.value) {
            gss_release_buffer(&min_stat, &mic_buf);
        }
    } while (1 /* loop will break if NOOP received */ );

    if (context != GSS_C_NO_CONTEXT) {
        /* Delete context */
        maj_stat = gss_delete_sec_context(&min_stat, &context, NULL);
        if (maj_stat != GSS_S_COMPLETE) {
            display_status("deleting context", maj_stat, min_stat);
            return (-1);
        }
    }

    if (logfile)
        fflush(logfile);

    return (0);
}

static int max_threads = 1;

#ifdef _WIN32
static  thread_count = 0;
static HANDLE hMutex = NULL;
static HANDLE hEvent = NULL;

void
InitHandles(void)
{
    hMutex = CreateMutex(NULL, FALSE, NULL);
    hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
}

void
CleanupHandles(void)
{
    CloseHandle(hMutex);
    CloseHandle(hEvent);
}

BOOL
WaitAndIncrementThreadCounter(void)
{
    for (;;) {
        if (WaitForSingleObject(hMutex, INFINITE) == WAIT_OBJECT_0) {
            if (thread_count < max_threads) {
                thread_count++;
                ReleaseMutex(hMutex);
                return TRUE;
            } else {
                ReleaseMutex(hMutex);

                if (WaitForSingleObject(hEvent, INFINITE) == WAIT_OBJECT_0) {
                    continue;
                } else {
                    return FALSE;
                }
            }
        } else {
            return FALSE;
        }
    }
}

BOOL
DecrementAndSignalThreadCounter(void)
{
    if (WaitForSingleObject(hMutex, INFINITE) == WAIT_OBJECT_0) {
        if (thread_count == max_threads)
            ResetEvent(hEvent);
        thread_count--;
        ReleaseMutex(hMutex);
        return TRUE;
    } else {
        return FALSE;
    }
}
#endif

struct _work_plan
{
    int     s;
    gss_cred_id_t server_creds;
    int     export;
};

static void
worker_bee(void *param)
{
    struct _work_plan *work = (struct _work_plan *) param;

    /* this return value is not checked, because there's
     * not really anything to do if it fails
     */
    sign_server(work->s, work->server_creds, work->export);
    closesocket(work->s);
    free(work);

#ifdef _WIN32
    if (max_threads > 1)
        DecrementAndSignalThreadCounter();
#endif
}

int
main(int argc, char **argv)
{
    char   *service_name;
    gss_cred_id_t server_creds;
    gss_OID mech = GSS_C_NO_OID;
    OM_uint32 min_stat;
    u_short port = 4444;
    int     once = 0;
    int     do_inetd = 0;
    int     export = 0;

    logfile = stdout;
    display_file = stdout;
    argc--;
    argv++;
    while (argc) {
        if (strcmp(*argv, "-port") == 0) {
            argc--;
            argv++;
            if (!argc)
                usage();
            port = atoi(*argv);
        }
#ifdef _WIN32
        else if (strcmp(*argv, "-threads") == 0) {
            argc--;
            argv++;
            if (!argc)
                usage();
            max_threads = atoi(*argv);
        }
#endif
        else if (strcmp(*argv, "-verbose") == 0) {
            verbose = 1;
        } else if (strcmp(*argv, "-once") == 0) {
            once = 1;
        } else if (strcmp(*argv, "-inetd") == 0) {
            do_inetd = 1;
        } else if (strcmp(*argv, "-export") == 0) {
            export = 1;
        } else if (strcmp(*argv, "-logfile") == 0) {
            argc--;
            argv++;
            if (!argc)
                usage();
            /* Gross hack, but it makes it unnecessary to add an
             * extra argument to disable logging, and makes the code
             * more efficient because it doesn't actually write data
             * to /dev/null. */
            if (!strcmp(*argv, "/dev/null")) {
                logfile = display_file = NULL;
            } else {
                logfile = fopen(*argv, "a");
                display_file = logfile;
                if (!logfile) {
                    perror(*argv);
                    exit(1);
                }
            }
        } else if (strcmp(*argv, "-keytab") == 0) {
            argc--;
            argv++;
            if (!argc)
                usage();
            if (krb5_gss_register_acceptor_identity(*argv)) {
                fprintf(stderr, "failed to register keytab\n");
                exit(1);
            }
        } else if (strcmp(*argv, "-iakerb") == 0) {
            mech = (gss_OID)gss_mech_iakerb;
        } else
            break;
        argc--;
        argv++;
    }
    if (argc != 1)
        usage();

    if ((*argv)[0] == '-')
        usage();

#ifdef _WIN32
    if (max_threads < 1) {
        fprintf(stderr, "warning: there must be at least one thread\n");
        max_threads = 1;
    }

    if (max_threads > 1 && do_inetd)
        fprintf(stderr,
                "warning: one thread may be used in conjunction with inetd\n");

    InitHandles();
#endif

    service_name = *argv;

    if (server_acquire_creds(service_name, mech, &server_creds) < 0)
        return -1;

    if (do_inetd) {
        close(1);
        close(2);

        sign_server(0, server_creds, export);
        close(0);
    } else {
        int     stmp;

        if ((stmp = create_socket(port)) >= 0) {
            fprintf(stderr, "starting...\n");

            do {
                struct _work_plan *work = malloc(sizeof(struct _work_plan));

                if (work == NULL) {
                    fprintf(stderr, "fatal error: out of memory");
                    break;
                }

                /* Accept a TCP connection */
                if ((work->s = accept(stmp, NULL, 0)) < 0) {
                    perror("accepting connection");
                    free(work);
                    continue;
                }

                work->server_creds = server_creds;
                work->export = export;

                if (max_threads == 1) {
                    worker_bee((void *) work);
                }
#ifdef _WIN32
                else {
                    if (WaitAndIncrementThreadCounter()) {
                        uintptr_t handle =
                            _beginthread(worker_bee, 0, (void *) work);
                        if (handle == (uintptr_t) - 1) {
                            closesocket(work->s);
                            free(work);
                        }
                    } else {
                        fprintf(stderr,
                                "fatal error incrementing thread counter");
                        closesocket(work->s);
                        free(work);
                        break;
                    }
                }
#endif
            } while (!once);

            closesocket(stmp);
        }
    }

    (void) gss_release_cred(&min_stat, &server_creds);

#ifdef _WIN32
    CleanupHandles();
#endif

    return 0;
}

static void
dumpAttribute(OM_uint32 *minor,
              gss_name_t name,
              gss_buffer_t attribute,
              int noisy)
{
    OM_uint32 major, tmp;
    gss_buffer_desc value;
    gss_buffer_desc display_value;
    int authenticated = 0;
    int complete = 0;
    int more = -1;
    unsigned int i;

    while (more != 0) {
        value.value = NULL;
        display_value.value = NULL;

        major = gss_get_name_attribute(minor, name, attribute, &authenticated,
                                       &complete, &value, &display_value,
                                       &more);
        if (GSS_ERROR(major)) {
            display_status("gss_get_name_attribute", major, *minor);
            break;
        }

        printf("Attribute %.*s %s %s\n\n%.*s\n",
               (int)attribute->length, (char *)attribute->value,
               authenticated ? "Authenticated" : "",
               complete ? "Complete" : "",
               (int)display_value.length, (char *)display_value.value);

        if (noisy) {
            for (i = 0; i < value.length; i++) {
                if ((i % 32) == 0)
                    printf("\n");
                printf("%02x", ((char *)value.value)[i] & 0xFF);
            }
            printf("\n\n");
        }

        gss_release_buffer(&tmp, &value);
        gss_release_buffer(&tmp, &display_value);
    }
}

static OM_uint32
enumerateAttributes(OM_uint32 *minor,
                    gss_name_t name,
                    int noisy)
{
    OM_uint32 major, tmp;
    int name_is_MN;
    gss_OID mech = GSS_C_NO_OID;
    gss_buffer_set_t attrs = GSS_C_NO_BUFFER_SET;
    unsigned int i;

    major = gss_inquire_name(minor, name, &name_is_MN, &mech, &attrs);
    if (GSS_ERROR(major)) {
        display_status("gss_inquire_name", major, *minor);
        return major;
    }

    if (attrs != GSS_C_NO_BUFFER_SET) {
        for (i = 0; i < attrs->count; i++)
            dumpAttribute(minor, name, &attrs->elements[i], noisy);
    }

    gss_release_oid(&tmp, &mech);
    gss_release_buffer_set(&tmp, &attrs);

    return major;
}

static OM_uint32
showLocalIdentity(OM_uint32 *minor, gss_name_t name)
{
    OM_uint32 major;
    gss_buffer_desc buf;

    major = gss_localname(minor, name, GSS_C_NO_OID, &buf);
    if (major == GSS_S_COMPLETE)
        printf("localname: %-*s\n", (int)buf.length, (char *)buf.value);
    else if (major != GSS_S_UNAVAILABLE)
        display_status("gss_localname", major, *minor);
    gss_release_buffer(minor, &buf);
    return major;
}
