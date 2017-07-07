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
 * Copyright (C) 2003, 2004, 2005 by the Massachusetts Institute of Technology.
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
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <winsock2.h>
#else
#include <assert.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif

#include <gssapi/gssapi_generic.h>
#include <gssapi/gssapi_krb5.h>
#include <gssapi/gssapi_ext.h>
#include "gss-misc.h"
#include "port-sockets.h"

static int verbose = 1;
static int spnego = 0;
static gss_OID_desc gss_spnego_mechanism_oid_desc =
{6, (void *)"\x2b\x06\x01\x05\x05\x02"};

static void
usage()
{
    fprintf(stderr, "Usage: gss-client [-port port] [-mech mechanism] "
            "[-spnego] [-d]\n");
    fprintf(stderr, "       [-seq] [-noreplay] [-nomutual] [-user user] "
            "[-pass pw]");
#ifdef _WIN32
    fprintf(stderr, " [-threads num]");
#endif
    fprintf(stderr, "\n");
    fprintf(stderr, "       [-f] [-q] [-ccount count] [-mcount count]\n");
    fprintf(stderr, "       [-v1] [-na] [-nw] [-nx] [-nm] host service msg\n");
    exit(1);
}

/*
 * Function: connect_to_server
 *
 * Purpose: Opens a TCP connection to the name host and port.
 *
 * Arguments:
 *
 *      host            (r) the target host name
 *      port            (r) the target port, in host byte order
 *
 * Returns: the established socket file desciptor, or -1 on failure
 *
 * Effects:
 *
 * The host name is resolved with gethostbyname(), and the socket is
 * opened and connected.  If an error occurs, an error message is
 * displayed and -1 is returned.
 */
static int
connect_to_server(char *host, u_short port)
{
    struct sockaddr_in saddr;
    struct hostent *hp;
    int     s;

#ifdef _WIN32
    WSADATA wsadata;
    int wsastartuperror = WSAStartup(0x202, &wsadata);
    if (wsastartuperror) {
        fprintf(stderr, "WSAStartup error: %x\n", wsastartuperror);
        return -1;
    }
#endif

    if ((hp = gethostbyname(host)) == NULL) {
        fprintf(stderr, "Unknown host: %s\n", host);
        return -1;
    }

    saddr.sin_family = hp->h_addrtype;
    memcpy(&saddr.sin_addr, hp->h_addr, sizeof(saddr.sin_addr));
    saddr.sin_port = htons(port);

    if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("creating socket");
        return -1;
    }
    if (connect(s, (struct sockaddr *) &saddr, sizeof(saddr)) < 0) {
        perror("connecting to server");
        (void) closesocket(s);
        return -1;
    }
    return s;
}

/*
 * Function: client_establish_context
 *
 * Purpose: establishes a GSS-API context with a specified service and
 * returns the context handle
 *
 * Arguments:
 *
 *      s                   (r) an established TCP connection to the service
 *      service_name(r) the ASCII service name of the service
 *      gss_flags       (r) GSS-API delegation flag (if any)
 *      auth_flag       (r) whether to actually do authentication
 *  v1_format   (r) whether the v1 sample protocol should be used
 *      oid                 (r) OID of the mechanism to use
 *      context         (w) the established GSS-API context
 *      ret_flags       (w) the returned flags from init_sec_context
 *
 * Returns: 0 on success, -1 on failure
 *
 * Effects:
 *
 * service_name is imported as a GSS-API name and a GSS-API context is
 * established with the corresponding service; the service should be
 * listening on the TCP connection s.  The default GSS-API mechanism
 * is used, and mutual authentication and replay detection are
 * requested.
 *
 * If successful, the context handle is returned in context.  If
 * unsuccessful, the GSS-API error messages are displayed on stderr
 * and -1 is returned.
 */
static int
client_establish_context(int s, char *service_name, OM_uint32 gss_flags,
                         int auth_flag, int v1_format, gss_OID oid,
                         char *username, char *password,
                         gss_ctx_id_t *gss_context, OM_uint32 *ret_flags)
{
    if (auth_flag) {
        gss_buffer_desc send_tok, recv_tok, *token_ptr;
        gss_name_t target_name;
        OM_uint32 maj_stat, min_stat, init_sec_min_stat;
        int token_flags;
        gss_cred_id_t cred = GSS_C_NO_CREDENTIAL;
        gss_name_t gss_username = GSS_C_NO_NAME;
        gss_OID_set_desc mechs, *mechsp = GSS_C_NO_OID_SET;

        if (spnego) {
            mechs.elements = &gss_spnego_mechanism_oid_desc;
            mechs.count = 1;
            mechsp = &mechs;
        } else if (oid != GSS_C_NO_OID) {
            mechs.elements = oid;
            mechs.count = 1;
            mechsp = &mechs;
        } else {
            mechs.elements = NULL;
            mechs.count = 0;
        }

        if (username != NULL) {
            send_tok.value = username;
            send_tok.length = strlen(username);

            maj_stat = gss_import_name(&min_stat, &send_tok,
                                       (gss_OID) gss_nt_user_name,
                                       &gss_username);
            if (maj_stat != GSS_S_COMPLETE) {
                display_status("parsing client name", maj_stat, min_stat);
                return -1;
            }
        }

        if (password != NULL) {
            gss_buffer_desc pwbuf;

            pwbuf.value = password;
            pwbuf.length = strlen(password);

            maj_stat = gss_acquire_cred_with_password(&min_stat,
                                                      gss_username,
                                                      &pwbuf, 0,
                                                      mechsp, GSS_C_INITIATE,
                                                      &cred, NULL, NULL);
        } else if (gss_username != GSS_C_NO_NAME) {
            maj_stat = gss_acquire_cred(&min_stat,
                                        gss_username, 0,
                                        mechsp, GSS_C_INITIATE,
                                        &cred, NULL, NULL);
        } else
            maj_stat = GSS_S_COMPLETE;
        if (maj_stat != GSS_S_COMPLETE) {
            display_status("acquiring creds", maj_stat, min_stat);
            gss_release_name(&min_stat, &gss_username);
            return -1;
        }
        if (spnego && oid != GSS_C_NO_OID) {
            gss_OID_set_desc neg_mechs;

            neg_mechs.elements = oid;
            neg_mechs.count = 1;

            maj_stat = gss_set_neg_mechs(&min_stat, cred, &neg_mechs);
            if (maj_stat != GSS_S_COMPLETE) {
                display_status("setting neg mechs", maj_stat, min_stat);
                gss_release_name(&min_stat, &gss_username);
                gss_release_cred(&min_stat, &cred);
                return -1;
            }
        }
        gss_release_name(&min_stat, &gss_username);

        /*
         * Import the name into target_name.  Use send_tok to save
         * local variable space.
         */
        send_tok.value = service_name;
        send_tok.length = strlen(service_name);
        maj_stat = gss_import_name(&min_stat, &send_tok,
                                   (gss_OID) gss_nt_service_name,
                                   &target_name);
        if (maj_stat != GSS_S_COMPLETE) {
            display_status("parsing name", maj_stat, min_stat);
            return -1;
        }

        if (!v1_format) {
            if (send_token(s, TOKEN_NOOP | TOKEN_CONTEXT_NEXT, empty_token) <
                0) {
                (void) gss_release_name(&min_stat, &target_name);
                return -1;
            }
        }

        /*
         * Perform the context-establishement loop.
         *
         * On each pass through the loop, token_ptr points to the token
         * to send to the server (or GSS_C_NO_BUFFER on the first pass).
         * Every generated token is stored in send_tok which is then
         * transmitted to the server; every received token is stored in
         * recv_tok, which token_ptr is then set to, to be processed by
         * the next call to gss_init_sec_context.
         *
         * GSS-API guarantees that send_tok's length will be non-zero
         * if and only if the server is expecting another token from us,
         * and that gss_init_sec_context returns GSS_S_CONTINUE_NEEDED if
         * and only if the server has another token to send us.
         */

        token_ptr = GSS_C_NO_BUFFER;
        *gss_context = GSS_C_NO_CONTEXT;

        do {
            maj_stat = gss_init_sec_context(&init_sec_min_stat,
                                            cred, gss_context,
                                            target_name, mechs.elements,
                                            gss_flags, 0,
                                            NULL, /* channel bindings */
                                            token_ptr, NULL, /* mech type */
                                            &send_tok, ret_flags,
                                            NULL);  /* time_rec */

            if (token_ptr != GSS_C_NO_BUFFER)
                free(recv_tok.value);

            if (send_tok.length != 0) {
                if (verbose)
                    printf("Sending init_sec_context token (size=%d)...",
                           (int) send_tok.length);
                if (send_token(s, v1_format ? 0 : TOKEN_CONTEXT, &send_tok) <
                    0) {
                    (void) gss_release_buffer(&min_stat, &send_tok);
                    (void) gss_release_name(&min_stat, &target_name);
                    return -1;
                }
            }
            (void) gss_release_buffer(&min_stat, &send_tok);

            if (maj_stat != GSS_S_COMPLETE
                && maj_stat != GSS_S_CONTINUE_NEEDED) {
                display_status("initializing context", maj_stat,
                               init_sec_min_stat);
                (void) gss_release_name(&min_stat, &target_name);
                (void) gss_release_cred(&min_stat, &cred);
                if (*gss_context != GSS_C_NO_CONTEXT)
                    gss_delete_sec_context(&min_stat, gss_context,
                                           GSS_C_NO_BUFFER);
                return -1;
            }

            if (maj_stat == GSS_S_CONTINUE_NEEDED) {
                if (verbose)
                    printf("continue needed...");
                if (recv_token(s, &token_flags, &recv_tok) < 0) {
                    (void) gss_release_name(&min_stat, &target_name);
                    return -1;
                }
                token_ptr = &recv_tok;
            }
            if (verbose)
                printf("\n");
        } while (maj_stat == GSS_S_CONTINUE_NEEDED);

        (void) gss_release_cred(&min_stat, &cred);
        (void) gss_release_name(&min_stat, &target_name);
    } else {
        if (send_token(s, TOKEN_NOOP, empty_token) < 0)
            return -1;
    }

    return 0;
}

static void
read_file(file_name, in_buf)
    char   *file_name;
    gss_buffer_t in_buf;
{
    int     fd, count;
    struct stat stat_buf;

    if ((fd = open(file_name, O_RDONLY, 0)) < 0) {
        perror("open");
        fprintf(stderr, "Couldn't open file %s\n", file_name);
        exit(1);
    }
    if (fstat(fd, &stat_buf) < 0) {
        perror("fstat");
        exit(1);
    }
    in_buf->length = stat_buf.st_size;

    if (in_buf->length == 0) {
        in_buf->value = NULL;
        return;
    }

    if ((in_buf->value = malloc(in_buf->length)) == 0) {
        fprintf(stderr, "Couldn't allocate %d byte buffer for reading file\n",
                (int) in_buf->length);
        exit(1);
    }

    /* this code used to check for incomplete reads, but you can't get
     * an incomplete read on any file for which fstat() is meaningful */

    count = read(fd, in_buf->value, in_buf->length);
    if (count < 0) {
        perror("read");
        exit(1);
    }
    if (count < (int)in_buf->length)
        fprintf(stderr, "Warning, only read in %d bytes, expected %d\n",
                count, (int) in_buf->length);
}

/*
 * Function: call_server
 *
 * Purpose: Call the "sign" service.
 *
 * Arguments:
 *
 *      host            (r) the host providing the service
 *      port            (r) the port to connect to on host
 *      service_name    (r) the GSS-API service name to authenticate to
 *      gss_flags       (r) GSS-API delegation flag (if any)
 *      auth_flag       (r) whether to do authentication
 *      wrap_flag       (r) whether to do message wrapping at all
 *      encrypt_flag    (r) whether to do encryption while wrapping
 *      mic_flag        (r) whether to request a MIC from the server
 *      msg             (r) the message to have "signed"
 *      use_file        (r) whether to treat msg as an input file name
 *      mcount          (r) the number of times to send the message
 *
 * Returns: 0 on success, -1 on failure
 *
 * Effects:
 *
 * call_server opens a TCP connection to <host:port> and establishes a
 * GSS-API context with service_name over the connection.  It then
 * seals msg in a GSS-API token with gss_wrap, sends it to the server,
 * reads back a GSS-API signature block for msg from the server, and
 * verifies it with gss_verify.  -1 is returned if any step fails,
 * otherwise 0 is returned.  */
static int
call_server(host, port, oid, service_name, gss_flags, auth_flag,
            wrap_flag, encrypt_flag, mic_flag, v1_format, msg, use_file,
            mcount, username, password)
    char   *host;
    u_short port;
    gss_OID oid;
    char   *service_name;
    OM_uint32 gss_flags;
    int     auth_flag, wrap_flag, encrypt_flag, mic_flag;
    int     v1_format;
    char   *msg;
    int     use_file;
    int     mcount;
    char    *username;
    char    *password;
{
    gss_ctx_id_t context = GSS_C_NO_CONTEXT;
    gss_buffer_desc in_buf, out_buf;
    int     s, state;
    OM_uint32 ret_flags;
    OM_uint32 maj_stat, min_stat;
    gss_name_t src_name, targ_name;
    gss_buffer_desc sname, tname;
    OM_uint32 lifetime;
    gss_OID mechanism, name_type;
    int     is_local;
    OM_uint32 context_flags;
    int     is_open;
    gss_qop_t qop_state;
    gss_OID_set mech_names;
    gss_buffer_desc oid_name;
    size_t  i;
    int     token_flags;

    /* Open connection */
    if ((s = connect_to_server(host, port)) < 0)
        return -1;

    /* Establish context */
    if (client_establish_context(s, service_name, gss_flags, auth_flag,
                                 v1_format, oid, username, password,
                                 &context, &ret_flags) < 0) {
        (void) closesocket(s);
        return -1;
    }

    if (auth_flag && verbose) {
        /* display the flags */
        display_ctx_flags(ret_flags);

        /* Get context information */
        maj_stat = gss_inquire_context(&min_stat, context,
                                       &src_name, &targ_name, &lifetime,
                                       &mechanism, &context_flags,
                                       &is_local, &is_open);
        if (maj_stat != GSS_S_COMPLETE) {
            display_status("inquiring context", maj_stat, min_stat);
            return -1;
        }

        maj_stat = gss_display_name(&min_stat, src_name, &sname, &name_type);
        if (maj_stat != GSS_S_COMPLETE) {
            display_status("displaying source name", maj_stat, min_stat);
            return -1;
        }
        maj_stat = gss_display_name(&min_stat, targ_name, &tname,
                                    (gss_OID *) NULL);
        if (maj_stat != GSS_S_COMPLETE) {
            display_status("displaying target name", maj_stat, min_stat);
            return -1;
        }
        printf("\"%.*s\" to \"%.*s\", lifetime %d, flags %x, %s, %s\n",
               (int) sname.length, (char *) sname.value,
               (int) tname.length, (char *) tname.value, lifetime,
               context_flags,
               (is_local) ? "locally initiated" : "remotely initiated",
               (is_open) ? "open" : "closed");

        (void) gss_release_name(&min_stat, &src_name);
        (void) gss_release_name(&min_stat, &targ_name);
        (void) gss_release_buffer(&min_stat, &sname);
        (void) gss_release_buffer(&min_stat, &tname);

        maj_stat = gss_oid_to_str(&min_stat, name_type, &oid_name);
        if (maj_stat != GSS_S_COMPLETE) {
            display_status("converting oid->string", maj_stat, min_stat);
            return -1;
        }
        printf("Name type of source name is %.*s.\n",
               (int) oid_name.length, (char *) oid_name.value);
        (void) gss_release_buffer(&min_stat, &oid_name);

        /* Now get the names supported by the mechanism */
        maj_stat = gss_inquire_names_for_mech(&min_stat,
                                              mechanism, &mech_names);
        if (maj_stat != GSS_S_COMPLETE) {
            display_status("inquiring mech names", maj_stat, min_stat);
            return -1;
        }

        maj_stat = gss_oid_to_str(&min_stat, mechanism, &oid_name);
        if (maj_stat != GSS_S_COMPLETE) {
            display_status("converting oid->string", maj_stat, min_stat);
            return -1;
        }
        printf("Mechanism %.*s supports %d names\n",
               (int) oid_name.length, (char *) oid_name.value,
               (int) mech_names->count);
        (void) gss_release_buffer(&min_stat, &oid_name);

        for (i = 0; i < mech_names->count; i++) {
            maj_stat = gss_oid_to_str(&min_stat,
                                      &mech_names->elements[i], &oid_name);
            if (maj_stat != GSS_S_COMPLETE) {
                display_status("converting oid->string", maj_stat, min_stat);
                return -1;
            }
            printf("  %d: %.*s\n", (int) i,
                   (int) oid_name.length, (char *) oid_name.value);

            (void) gss_release_buffer(&min_stat, &oid_name);
        }
        (void) gss_release_oid_set(&min_stat, &mech_names);
    }

    if (use_file) {
        read_file(msg, &in_buf);
    } else {
        /* Seal the message */
        in_buf.value = msg;
        in_buf.length = strlen((char *)in_buf.value);
    }

    for (i = 0; i < (size_t)mcount; i++) {
        if (wrap_flag) {
            maj_stat =
                gss_wrap(&min_stat, context, encrypt_flag, GSS_C_QOP_DEFAULT,
                         &in_buf, &state, &out_buf);
            if (maj_stat != GSS_S_COMPLETE) {
                display_status("wrapping message", maj_stat, min_stat);
                (void) closesocket(s);
                (void) gss_delete_sec_context(&min_stat, &context,
                                              GSS_C_NO_BUFFER);
                return -1;
            } else if (encrypt_flag && !state) {
                fprintf(stderr, "Warning!  Message not encrypted.\n");
            }
        } else {
            out_buf = in_buf;
        }

        /* Send to server */
        if (send_token(s, (v1_format ? 0
                           : (TOKEN_DATA |
                              (wrap_flag ? TOKEN_WRAPPED : 0) |
                              (encrypt_flag ? TOKEN_ENCRYPTED : 0) |
                              (mic_flag ? TOKEN_SEND_MIC : 0))),
                       &out_buf) < 0) {
            (void) closesocket(s);
            (void) gss_delete_sec_context(&min_stat, &context,
                                          GSS_C_NO_BUFFER);
            return -1;
        }
        if (out_buf.value != in_buf.value)
            (void) gss_release_buffer(&min_stat, &out_buf);

        /* Read signature block into out_buf */
        if (recv_token(s, &token_flags, &out_buf) < 0) {
            (void) closesocket(s);
            (void) gss_delete_sec_context(&min_stat, &context,
                                          GSS_C_NO_BUFFER);
            return -1;
        }

        if (mic_flag) {
            /* Verify signature block */
            maj_stat = gss_verify_mic(&min_stat, context, &in_buf,
                                      &out_buf, &qop_state);
            if (maj_stat != GSS_S_COMPLETE) {
                display_status("verifying signature", maj_stat, min_stat);
                (void) closesocket(s);
                (void) gss_delete_sec_context(&min_stat, &context,
                                              GSS_C_NO_BUFFER);
                return -1;
            }

            if (verbose)
                printf("Signature verified.\n");
        } else {
            if (verbose)
                printf("Response received.\n");
        }

        free(out_buf.value);
    }

    if (use_file)
        free(in_buf.value);

    /* Send NOOP */
    if (!v1_format)
        (void) send_token(s, TOKEN_NOOP, empty_token);

    if (auth_flag) {
        /* Delete context */
        maj_stat = gss_delete_sec_context(&min_stat, &context, &out_buf);
        if (maj_stat != GSS_S_COMPLETE) {
            display_status("deleting context", maj_stat, min_stat);
            (void) closesocket(s);
            (void) gss_delete_sec_context(&min_stat, &context,
                                          GSS_C_NO_BUFFER);
            return -1;
        }

        (void) gss_release_buffer(&min_stat, &out_buf);
    }

    (void) closesocket(s);

    return 0;
}

static void
parse_oid(char *mechanism, gss_OID * oid)
{
    char   *mechstr = 0;
    gss_buffer_desc tok;
    OM_uint32 maj_stat, min_stat;
    size_t i, mechlen = strlen(mechanism);

    if (isdigit((int) mechanism[0])) {
        mechstr = malloc(mechlen + 5);
        if (!mechstr) {
            fprintf(stderr, "Couldn't allocate mechanism scratch!\n");
            return;
        }
        mechstr[0] = '{';
        mechstr[1] = ' ';
        for (i = 0; i < mechlen; i++)
            mechstr[i + 2] = (mechanism[i] == '.') ? ' ' : mechanism[i];
        mechstr[mechlen + 2] = ' ';
        mechstr[mechlen + 3] = ' ';
        mechstr[mechlen + 4] = '\0';
        tok.value = mechstr;
    } else
        tok.value = mechanism;
    tok.length = strlen(tok.value);
    maj_stat = gss_str_to_oid(&min_stat, &tok, oid);
    if (maj_stat != GSS_S_COMPLETE) {
        display_status("str_to_oid", maj_stat, min_stat);
        return;
    }
    if (mechstr)
        free(mechstr);
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

static char *service_name, *server_host, *msg;
static char *mechanism = 0;
static u_short port = 4444;
static int use_file = 0;
static OM_uint32 gss_flags = GSS_C_MUTUAL_FLAG | GSS_C_REPLAY_FLAG;
static OM_uint32 min_stat;
static gss_OID oid = GSS_C_NULL_OID;
static int mcount = 1, ccount = 1;
static int auth_flag, wrap_flag, encrypt_flag, mic_flag, v1_format;
static char *username = NULL;
static char *password = NULL;

static void
worker_bee(void *unused)
{
    if (call_server(server_host, port, oid, service_name,
                    gss_flags, auth_flag, wrap_flag, encrypt_flag, mic_flag,
                    v1_format, msg, use_file, mcount, username, password) < 0)
        exit(1);

#ifdef _WIN32
    if (max_threads > 1)
        DecrementAndSignalThreadCounter();
#endif
}

int
main(argc, argv)
    int     argc;
    char  **argv;
{
    int     i;

    display_file = stdout;
    auth_flag = wrap_flag = encrypt_flag = mic_flag = 1;
    v1_format = 0;

    /* Parse arguments. */
    argc--;
    argv++;
    while (argc) {
        if (strcmp(*argv, "-port") == 0) {
            argc--;
            argv++;
            if (!argc)
                usage();
            port = atoi(*argv);
        } else if (strcmp(*argv, "-mech") == 0) {
            argc--;
            argv++;
            if (!argc)
                usage();
            mechanism = *argv;
        } else if (strcmp(*argv, "-user") == 0) {
            argc--;
            argv++;
            if (!argc)
                usage();
            username = *argv;
        } else if (strcmp(*argv, "-pass") == 0) {
            argc--;
            argv++;
            if (!argc)
                usage();
            password = *argv;
        } else if (strcmp(*argv, "-iakerb") == 0) {
            mechanism = "{ 1 3 6 1 5 2 5 }";
        } else if (strcmp(*argv, "-spnego") == 0) {
            spnego = 1;
        } else if (strcmp(*argv, "-krb5") == 0) {
            mechanism = "{ 1 2 840 113554 1 2 2 }";
#ifdef _WIN32
        } else if (strcmp(*argv, "-threads") == 0) {
            argc--;
            argv++;
            if (!argc)
                usage();
            max_threads = atoi(*argv);
#endif
        } else if (strcmp(*argv, "-dce") == 0) {
            gss_flags |= GSS_C_DCE_STYLE;
        } else if (strcmp(*argv, "-d") == 0) {
            gss_flags |= GSS_C_DELEG_FLAG;
        } else if (strcmp(*argv, "-seq") == 0) {
            gss_flags |= GSS_C_SEQUENCE_FLAG;
        } else if (strcmp(*argv, "-noreplay") == 0) {
            gss_flags &= ~GSS_C_REPLAY_FLAG;
        } else if (strcmp(*argv, "-nomutual") == 0) {
            gss_flags &= ~GSS_C_MUTUAL_FLAG;
        } else if (strcmp(*argv, "-f") == 0) {
            use_file = 1;
        } else if (strcmp(*argv, "-q") == 0) {
            verbose = 0;
        } else if (strcmp(*argv, "-ccount") == 0) {
            argc--;
            argv++;
            if (!argc)
                usage();
            ccount = atoi(*argv);
            if (ccount <= 0)
                usage();
        } else if (strcmp(*argv, "-mcount") == 0) {
            argc--;
            argv++;
            if (!argc)
                usage();
            mcount = atoi(*argv);
            if (mcount < 0)
                usage();
        } else if (strcmp(*argv, "-na") == 0) {
            auth_flag = wrap_flag = encrypt_flag = mic_flag = 0;
        } else if (strcmp(*argv, "-nw") == 0) {
            wrap_flag = 0;
        } else if (strcmp(*argv, "-nx") == 0) {
            encrypt_flag = 0;
        } else if (strcmp(*argv, "-nm") == 0) {
            mic_flag = 0;
        } else if (strcmp(*argv, "-v1") == 0) {
            v1_format = 1;
        } else
            break;
        argc--;
        argv++;
    }
    if (argc != 3)
        usage();

#ifdef _WIN32
    if (max_threads < 1) {
        fprintf(stderr, "warning: there must be at least one thread\n");
        max_threads = 1;
    }
#endif

    server_host = *argv++;
    service_name = *argv++;
    msg = *argv++;

    if (mechanism)
        parse_oid(mechanism, &oid);

    if (max_threads == 1) {
        for (i = 0; i < ccount; i++) {
            worker_bee(0);
        }
    } else {
#ifdef _WIN32
        for (i = 0; i < ccount; i++) {
            if (WaitAndIncrementThreadCounter()) {
                uintptr_t handle = _beginthread(worker_bee, 0, (void *) 0);
                if (handle == (uintptr_t) - 1) {
                    exit(1);
                }
            } else {
                exit(1);
            }
        }
#else
        /* boom */
        assert(max_threads == 1);
#endif
    }

    if (oid != GSS_C_NULL_OID)
        (void) gss_release_oid(&min_stat, &oid);

#ifdef _WIN32
    CleanupHandles();
#endif

    return 0;
}
