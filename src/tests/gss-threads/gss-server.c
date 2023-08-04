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
 * Copyright (C) 2004,2008 by the Massachusetts Institute of Technology.
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

#include "autoconf.h"
#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#include <winsock.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdlib.h>
#include <ctype.h>

#include <gssapi/gssapi_generic.h>
#include "gss-misc.h"
#include "port-sockets.h"

#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

static void
usage()
{
    fprintf(stderr, "Usage: gss-server [-port port] [-verbose] [-once]");
#ifdef _WIN32
    fprintf(stderr, " [-threads num]");
#endif
    fprintf(stderr, "\n");
    fprintf(stderr, "       [-inetd] [-export] [-logfile file] "
            "service_name\n");
    exit(1);
}

FILE *logfile;

int verbose = 0;

/*
 * Function: server_acquire_creds
 *
 * Purpose: imports a service name and acquires credentials for it
 *
 * Arguments:
 *
 *      service_name    (r) the ASCII service name
 *      server_creds    (w) the GSS-API service credentials
 *
 * Returns: 0 on success, -1 on failure
 *
 * Effects:
 *
 * The service name is imported with gss_import_name, and service
 * credentials are acquired with gss_acquire_cred.  If either operation
 * fails, an error message is displayed and -1 is returned; otherwise,
 * 0 is returned.
 */
static int
server_acquire_creds(char *service_name, gss_cred_id_t *server_creds)
{
    gss_buffer_desc name_buf;
    gss_name_t server_name;
    OM_uint32 maj_stat, min_stat;

    name_buf.value = service_name;
    name_buf.length = strlen(name_buf.value) + 1;
    maj_stat = gss_import_name(&min_stat, &name_buf,
                               (gss_OID)gss_nt_service_name, &server_name);
    if (maj_stat != GSS_S_COMPLETE) {
        display_status("importing name", maj_stat, min_stat);
        return -1;
    }

    maj_stat = gss_acquire_cred(&min_stat, server_name, 0,
                                GSS_C_NULL_OID_SET, GSS_C_ACCEPT,
                                server_creds, NULL, NULL);
    if (maj_stat != GSS_S_COMPLETE) {
        display_status("acquiring credentials", maj_stat, min_stat);
        return -1;
    }

    (void)gss_release_name(&min_stat, &server_name);

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
    gss_buffer_desc send_tok, recv_tok, oid_name;
    gss_name_t client;
    gss_OID doid;
    OM_uint32 maj_stat, min_stat, acc_sec_min_stat;
    int token_flags;

    if (recv_token(s, &token_flags, &recv_tok) < 0)
        return -1;

    if (recv_tok.value) {
        free(recv_tok.value);
        recv_tok.value = NULL;
    }

    if (!(token_flags & TOKEN_NOOP)) {
        if (logfile) {
            fprintf(logfile, "Expected NOOP token, got %d token instead\n",
                    token_flags);
        }
        return -1;
    }

    *context = GSS_C_NO_CONTEXT;

    if (token_flags & TOKEN_CONTEXT_NEXT) {
        do {
            if (recv_token(s, &token_flags, &recv_tok) < 0)
                return -1;

            if (verbose && logfile) {
                fprintf(logfile, "Received token (size=%d): \n",
                        (int)recv_tok.length);
                print_token(&recv_tok);
            }

            maj_stat = gss_accept_sec_context(&acc_sec_min_stat, context,
                                              server_creds, &recv_tok,
                                              GSS_C_NO_CHANNEL_BINDINGS,
                                              &client, &doid, &send_tok,
                                              ret_flags, NULL, NULL);

            if (recv_tok.value) {
                free(recv_tok.value);
                recv_tok.value = NULL;
            }

            if (send_tok.length != 0) {
                if (verbose && logfile) {
                    fprintf(logfile,
                            "Sending accept_sec_context token (size=%d):\n",
                            (int)send_tok.length);
                    print_token(&send_tok);
                }
                if (send_token(s, TOKEN_CONTEXT, &send_tok) < 0) {
                    if (logfile)
                        fprintf(logfile, "failure sending token\n");
                    return -1;
                }

                (void)gss_release_buffer(&min_stat, &send_tok);
            }
            if (maj_stat != GSS_S_COMPLETE &&
                maj_stat != GSS_S_CONTINUE_NEEDED) {
                display_status("accepting context", maj_stat,
                               acc_sec_min_stat);
                if (*context != GSS_C_NO_CONTEXT) {
                    gss_delete_sec_context(&min_stat, context,
                                           GSS_C_NO_BUFFER);
                }
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
                    (int)oid_name.length, (char *)oid_name.value);
            (void)gss_release_buffer(&min_stat, &oid_name);
        }

        maj_stat = gss_display_name(&min_stat, client, client_name, &doid);
        if (maj_stat != GSS_S_COMPLETE) {
            display_status("displaying name", maj_stat, min_stat);
            return -1;
        }
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
    int s, on = 1;

    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(port);
    saddr.sin_addr.s_addr = INADDR_ANY;

    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        perror("creating socket");
        return -1;
    }
    /* Let the socket be reused right away. */
    (void)setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on));
    if (bind(s, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
        perror("binding socket");
        (void)close(s);
        return -1;
    }
    if (listen(s, 5) < 0) {
        perror("listening on socket");
        (void)close(s);
        return -1;
    }
    return s;
}

static float
timeval_subtract(struct timeval *tv1, struct timeval *tv2)
{
    return ((tv1->tv_sec - tv2->tv_sec) +
            ((float)(tv1->tv_usec - tv2->tv_usec)) / 1000000);
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

    /* Attempt to save and then restore the context. */
    gettimeofday(&tm1, (struct timezone *)0);
    maj_stat = gss_export_sec_context(&min_stat, context, &context_token);
    if (maj_stat != GSS_S_COMPLETE) {
        display_status("exporting context", maj_stat, min_stat);
        return 1;
    }
    gettimeofday(&tm2, NULL);
    if (verbose && logfile) {
        fprintf(logfile, "Exported context: %d bytes, %7.4f seconds\n",
                (int)context_token.length, timeval_subtract(&tm2, &tm1));
    }
    copied_token.length = context_token.length;
    copied_token.value = malloc(context_token.length);
    if (copied_token.value == 0) {
        if (logfile) {
            fprintf(logfile, "Couldn't allocate memory to copy context "
                    "token.\n");
        }
        return 1;
    }
    memcpy(copied_token.value, context_token.value, copied_token.length);
    maj_stat = gss_import_sec_context(&min_stat, &copied_token, context);
    if (maj_stat != GSS_S_COMPLETE) {
        display_status("importing context", maj_stat, min_stat);
        return 1;
    }
    free(copied_token.value);
    gettimeofday(&tm1, NULL);
    if (verbose && logfile) {
        fprintf(logfile, "Importing context: %7.4f seconds\n",
                timeval_subtract(&tm1, &tm2));
    }
    (void)gss_release_buffer(&min_stat, &context_token);
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
    gss_buffer_desc client_name, xmit_buf, msg_buf;
    gss_ctx_id_t context;
    OM_uint32 maj_stat, min_stat, ret_flags;
    int i, conf_state, token_flags;
    char *cp;

    /* Establish a context with the client */
    if (server_establish_context(s, server_creds, &context, &client_name,
                                 &ret_flags) < 0)
        return -1;

    if (context == GSS_C_NO_CONTEXT) {
        printf("Accepted unauthenticated connection.\n");
    } else {
        printf("Accepted connection: \"%.*s\"\n", (int)client_name.length,
               (char *)client_name.value);
        (void)gss_release_buffer(&min_stat, &client_name);

        if (export) {
            for (i = 0; i < 3; i++) {
                if (test_import_export_context(&context))
                    return -1;
            }
        }
    }

    do {
        /* Receive the message token */
        if (recv_token(s, &token_flags, &xmit_buf) < 0)
            return -1;

        if (token_flags & TOKEN_NOOP) {
            if (logfile)
                fprintf(logfile, "NOOP token\n");
            if (xmit_buf.value) {
                free(xmit_buf.value);
                xmit_buf.value = 0;
            }
            break;
        }

        if (verbose && logfile) {
            fprintf(logfile, "Message token (flags=%d):\n", token_flags);
            print_token(&xmit_buf);
        }

        if (context == GSS_C_NO_CONTEXT &&
            (token_flags &
             (TOKEN_WRAPPED | TOKEN_ENCRYPTED | TOKEN_SEND_MIC))) {
            if (logfile) {
                fprintf(logfile, "Unauthenticated client requested "
                        "authenticated services!\n");
            }
            if (xmit_buf.value) {
                free(xmit_buf.value);
                xmit_buf.value = 0;
            }
            return -1;
        }

        if (token_flags & TOKEN_WRAPPED) {
            maj_stat = gss_unwrap(&min_stat, context, &xmit_buf, &msg_buf,
                                  &conf_state, NULL);
            if (maj_stat != GSS_S_COMPLETE) {
                display_status("unsealing message", maj_stat, min_stat);
                if (xmit_buf.value) {
                    free(xmit_buf.value);
                    xmit_buf.value = 0;
                }
                return -1;
            } else if (!conf_state && (token_flags & TOKEN_ENCRYPTED)) {
                fprintf(stderr, "Warning!  Message not encrypted.\n");
            }

            if (xmit_buf.value) {
                free(xmit_buf.value);
                xmit_buf.value = 0;
            }
        } else {
            msg_buf = xmit_buf;
        }

        if (logfile) {
            fprintf(logfile, "Received message: ");
            cp = msg_buf.value;
            if (isprint((unsigned char)cp[0]) &&
                isprint((unsigned char)cp[1])) {
                fprintf(logfile, "\"%.*s\"\n", (int)msg_buf.length,
                        (char *)msg_buf.value);
            } else {
                fprintf(logfile, "\n");
                print_token(&msg_buf);
            }
        }

        if (token_flags & TOKEN_SEND_MIC) {
            /* Produce a signature block for the message. */
            maj_stat = gss_get_mic(&min_stat, context, GSS_C_QOP_DEFAULT,
                                   &msg_buf, &xmit_buf);
            if (maj_stat != GSS_S_COMPLETE) {
                display_status("signing message", maj_stat, min_stat);
                return -1;
            }

            if (msg_buf.value) {
                free(msg_buf.value);
                msg_buf.value = 0;
            }

            /* Send the signature block to the client. */
            if (send_token(s, TOKEN_MIC, &xmit_buf) < 0)
                return -1;

            if (xmit_buf.value) {
                free(xmit_buf.value);
                xmit_buf.value = 0;
            }
        } else {
            if (msg_buf.value) {
                free(msg_buf.value);
                msg_buf.value = 0;
            }
            if (send_token(s, TOKEN_NOOP, empty_token) < 0)
                return -1;
        }
    } while (1 /* loop will break if NOOP received */);

    if (context != GSS_C_NO_CONTEXT) {
        /* Delete context. */
        maj_stat = gss_delete_sec_context(&min_stat, &context, NULL);
        if (maj_stat != GSS_S_COMPLETE) {
            display_status("deleting context", maj_stat, min_stat);
            return -1;
        }
    }

    if (logfile)
        fflush(logfile);

    return 0;
}

static int max_threads = 1;

#ifdef _WIN32
static thread_count = 0;
static HANDLE hMutex = NULL;
static HANDLE hEvent = NULL;

void
init_handles(void)
{
    hMutex = CreateMutex(NULL, FALSE, NULL);
    hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
}

void
cleanup_handles(void)
{
    CloseHandle(hMutex);
    CloseHandle(hEvent);
}

BOOL
wait_and_increment_thread_counter(void)
{
    for (;;) {
        if (WaitForSingleObject(hMutex, INFINITE) == WAIT_OBJECT_0) {
            if (thread_count < max_threads) {
                thread_count++;
                ReleaseMutex(hMutex);
                return TRUE;
            } else {
                ReleaseMutex(hMutex);

                if (WaitForSingleObject(hEvent, INFINITE) == WAIT_OBJECT_0)
                    continue;
                else
                    return FALSE;
            }
        } else {
            return FALSE;
        }
    }
}

BOOL
decrement_and_signal_thread_counter(void)
{
    if (WaitForSingleObject(hMutex, INFINITE) == WAIT_OBJECT_0) {
        if (thread_count == max_threads)
            SetEvent(hEvent);
        thread_count--;
        ReleaseMutex(hMutex);
        return TRUE;
    } else {
        return FALSE;
    }
}

#else /* assume pthread */

static pthread_mutex_t counter_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t counter_cond = PTHREAD_COND_INITIALIZER;
int counter = 0;

static int
wait_and_increment_thread_counter(void)
{
    int err;

    err = pthread_mutex_lock(&counter_mutex);
    if (err) {
        perror("pthread_mutex_lock");
        return 0;
    }
    if (counter == max_threads) {
        err = pthread_cond_wait(&counter_cond, &counter_mutex);
        if (err) {
            pthread_mutex_unlock(&counter_mutex);
            perror("pthread_cond_wait");
            return 0;
        }
    }
    counter++;
    pthread_mutex_unlock(&counter_mutex);
    return 1;
}

static void
decrement_and_signal_thread_counter(void)
{
    int err;

    err = pthread_mutex_lock(&counter_mutex);
    if (err) {
        perror("pthread_mutex_lock");
        return;
    }
    if (counter == max_threads)
        pthread_cond_broadcast(&counter_cond);
    counter--;
    pthread_mutex_unlock(&counter_mutex);
}

#endif

struct _work_plan {
    int s;
    gss_cred_id_t server_creds;
    int export;
};

static void *
worker_bee(void *param)
{
    struct _work_plan *work = param;

    /* This return value is not checked, because there's not really anything to
     * do if it fails. */
    sign_server(work->s, work->server_creds, work->export);
    closesocket(work->s);
    free(work);

#if defined _WIN32 || 1
    if (max_threads > 1)
        decrement_and_signal_thread_counter();
#endif
    return NULL;
}

int
main(int argc, char **argv)
{
    char *service_name;
    gss_cred_id_t server_creds;
    OM_uint32 min_stat;
    u_short port = 4444;
    int once = 0;
    int do_inetd = 0;
    int export = 0;

    signal(SIGPIPE, SIG_IGN);
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
        } else if (strcmp(*argv, "-threads") == 0) {
            argc--;
            argv++;
            if (!argc)
                usage();
            max_threads = atoi(*argv);
        } else if (strcmp(*argv, "-verbose") == 0) {
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
            /*
             * Gross hack, but it makes it unnecessary to add an extra argument
             * to disable logging, and makes the code more efficient because it
             * doesn't actually write data to /dev/null.
             */
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
        } else {
            break;
        }
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

    if (max_threads > 1 && do_inetd) {
        fprintf(stderr, "warning: one thread may be used in conjunction "
                "with inetd\n");
    }

    init_handles();
#endif

    service_name = *argv;

    if (server_acquire_creds(service_name, &server_creds) < 0)
        return -1;

    if (do_inetd) {
        close(1);
        close(2);

        sign_server(0, server_creds, export);
        close(0);
    } else {
        int stmp;

        stmp = create_socket(port);
        if (stmp >= 0) {
            do {
                struct _work_plan * work = malloc(sizeof(struct _work_plan));

                if (work == NULL) {
                    fprintf(stderr, "fatal error: out of memory");
                    break;
                }

                /* Accept a TCP connection */
                work->s = accept(stmp, NULL, 0);
                if (work->s < 0) {
                    perror("accepting connection");
                    continue;
                }

                work->server_creds = server_creds;
                work->export = export;

                if (max_threads == 1) {
                    worker_bee(work);
                } else {
                    if (wait_and_increment_thread_counter()) {
#ifdef _WIN32
                        uintptr_t handle = _beginthread(worker_bee, 0, work);
                        if (handle == (uintptr_t)-1) {
                            closesocket(work->s);
                            free(work);
                        }
#else
                        int err;
                        pthread_t thr;
                        err = pthread_create(&thr, 0, worker_bee, work);
                        if (err) {
                            perror("pthread_create");
                            closesocket(work->s);
                            free(work);
                        }
                        (void)pthread_detach(thr);
#endif
                    } else {
                        fprintf(stderr, "fatal error incrementing thread "
                                "counter");
                        closesocket(work->s);
                        free(work);
                        break;
                    }
                }
            } while (!once);

            closesocket(stmp);
        }
    }

    (void)gss_release_cred(&min_stat, &server_creds);

#ifdef _WIN32
    cleanup_handles();
#else
    if (max_threads > 1) {
        while (1)
            sleep(999999);
    }
#endif

    return 0;
}
