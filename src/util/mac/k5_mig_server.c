/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* util/mac/k5_mig_server.c */
/*
 * Copyright 2006 Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 * require a specific license from the United States Government.
 * It is the responsibility of any person or organization contemplating
 * export to obtain such a license before exporting.
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

#include "k5_mig_server.h"

#include <syslog.h>
#include "k5_mig_requestServer.h"
#include "k5_mig_reply.h"

#include <CoreFoundation/CoreFoundation.h>
#include <mach/mach.h>
#include <servers/bootstrap.h>
#include <string.h>

/* Global variables for servers (used by k5_ipc_request_demux) */
static mach_port_t g_service_port = MACH_PORT_NULL;
static mach_port_t g_notify_port = MACH_PORT_NULL;
static mach_port_t g_listen_port_set = MACH_PORT_NULL;
static boolean_t g_ready_to_quit = 0;


/* ------------------------------------------------------------------------ */

static boolean_t k5_ipc_request_demux (mach_msg_header_t *request,
                                       mach_msg_header_t *reply)
{
    boolean_t handled = 0;

    if (!handled) {
        handled = k5_ipc_request_server (request, reply);
    }

    /* Our session has a send right. If that goes away it's time to quit. */
    if (!handled && (request->msgh_id == MACH_NOTIFY_NO_SENDERS &&
                     request->msgh_local_port == g_notify_port)) {
        g_ready_to_quit = 1;
        handled = 1;
    }

    /* Check here for a client death.  If so remove it */
    if (!handled && request->msgh_id == MACH_NOTIFY_NO_SENDERS) {
        kern_return_t err = KERN_SUCCESS;

        err = k5_ipc_server_remove_client (request->msgh_local_port);

        if (!err) {
            err = mach_port_mod_refs (mach_task_self (),
                                      request->msgh_local_port,
                                      MACH_PORT_RIGHT_RECEIVE, -1);
        }

        if (!err) {
            handled = 1;  /* was a port we are tracking */
        }
    }

    return handled;
}

/* ------------------------------------------------------------------------ */

kern_return_t k5_ipc_server_create_client_connection (mach_port_t    in_server_port,
                                                      mach_port_t   *out_connection_port)
{
    kern_return_t err = KERN_SUCCESS;
    mach_port_t connection_port = MACH_PORT_NULL;
    mach_port_t old_notification_target = MACH_PORT_NULL;

    if (!err) {
        err = mach_port_allocate (mach_task_self (),
                                  MACH_PORT_RIGHT_RECEIVE, &connection_port);
    }

    if (!err) {
        err = mach_port_move_member (mach_task_self (),
                                     connection_port, g_listen_port_set);
    }

    if (!err) {
        /* request no-senders notification so we can tell when client quits/crashes */
        err = mach_port_request_notification (mach_task_self (),
                                              connection_port,
                                              MACH_NOTIFY_NO_SENDERS, 1,
                                              connection_port,
                                              MACH_MSG_TYPE_MAKE_SEND_ONCE,
                                              &old_notification_target );
    }

    if (!err) {
        err = k5_ipc_server_add_client (connection_port);
    }

    if (!err) {
        *out_connection_port = connection_port;
        connection_port = MACH_PORT_NULL;
    }

    if (MACH_PORT_VALID (connection_port)) { mach_port_deallocate (mach_task_self (), connection_port); }

    return err;
}

/* ------------------------------------------------------------------------ */

kern_return_t k5_ipc_server_request (mach_port_t             in_connection_port,
                                     mach_port_t             in_reply_port,
                                     k5_ipc_inl_request_t    in_inl_request,
                                     mach_msg_type_number_t  in_inl_requestCnt,
                                     k5_ipc_ool_request_t    in_ool_request,
                                     mach_msg_type_number_t  in_ool_requestCnt)
{
    kern_return_t err = KERN_SUCCESS;
    k5_ipc_stream request_stream = NULL;

    if (!err) {
        err = krb5int_ipc_stream_new (&request_stream);
    }

    if (!err) {
        if (in_inl_requestCnt) {
            err = krb5int_ipc_stream_write (request_stream, in_inl_request, in_inl_requestCnt);

        } else if (in_ool_requestCnt) {
            err = krb5int_ipc_stream_write (request_stream, in_ool_request, in_ool_requestCnt);

        } else {
            err = EINVAL;
        }
    }

    if (!err) {
        err = k5_ipc_server_handle_request (in_connection_port, in_reply_port, request_stream);
    }

    krb5int_ipc_stream_release (request_stream);
    if (in_ool_requestCnt) { vm_deallocate (mach_task_self (), (vm_address_t) in_ool_request, in_ool_requestCnt); }

    return err;
}

/* ------------------------------------------------------------------------ */

static kern_return_t k5_ipc_server_get_lookup_and_service_names (char **out_lookup,
                                                                 char **out_service)
{
    kern_return_t err = KERN_SUCCESS;
    CFBundleRef bundle = NULL;
    CFStringRef id_string = NULL;
    CFIndex len = 0;
    char *service_id = NULL;
    char *lookup = NULL;
    char *service = NULL;

    if (!out_lookup ) { err = EINVAL; }
    if (!out_service) { err = EINVAL; }

    if (!err) {
        bundle = CFBundleGetMainBundle ();
        if (!bundle) { err = ENOENT; }
    }

    if (!err) {
        id_string = CFBundleGetIdentifier (bundle);
        if (!id_string) { err = ENOMEM; }
    }

    if (!err) {
        len = CFStringGetMaximumSizeForEncoding (CFStringGetLength (id_string),
                                                 kCFStringEncodingUTF8) + 1;
    }

    if (!err) {
        service_id = calloc (len, sizeof (char));
        if (!service_id) { err = errno; }
    }

    if (!err && !CFStringGetCString (id_string, service_id, len,
                                     kCFStringEncodingUTF8)) {
        err = ENOMEM;
    }

    if (!err) {
        int w = asprintf (&lookup, "%s%s", service_id, K5_MIG_LOOKUP_SUFFIX);
        if (w < 0) { err = ENOMEM; }
    }

    if (!err) {
        int w = asprintf (&service, "%s%s", service_id, K5_MIG_SERVICE_SUFFIX);
        if (w < 0) { err = ENOMEM; }
    }

    if (!err) {
        *out_lookup = lookup;
        lookup = NULL;
        *out_service = service;
        service = NULL;
    }

    free (service);
    free (lookup);
    free (service_id);

    return err;
}

#pragma mark -

/* ------------------------------------------------------------------------ */

int32_t k5_ipc_server_listen_loop (void)
{
    /* Run the Mach IPC listen loop.
     * This will call k5_ipc_server_create_client_connection for new clients
     * and k5_ipc_server_request for existing clients */

    kern_return_t  err = KERN_SUCCESS;
    char          *service = NULL;
    char          *lookup = NULL;
    mach_port_t    lookup_port = MACH_PORT_NULL;
    mach_port_t    boot_port = MACH_PORT_NULL;
    mach_port_t    previous_notify_port = MACH_PORT_NULL;

    if (!err) {
        err = k5_ipc_server_get_lookup_and_service_names (&lookup, &service);
    }

    if (!err) {
        /* Get the bootstrap port */
        err = task_get_bootstrap_port (mach_task_self (), &boot_port);
    }

    if (!err) {
        /* We are an on-demand server so our lookup port already exists. */
        err = bootstrap_check_in (boot_port, lookup, &lookup_port);
    }

    if (!err) {
        /* We are an on-demand server so our service port already exists. */
        err = bootstrap_check_in (boot_port, service, &g_service_port);
    }

    if (!err) {
        /* Create the port set that the server will listen on */
        err = mach_port_allocate (mach_task_self (), MACH_PORT_RIGHT_RECEIVE,
                                  &g_notify_port);
    }

    if (!err) {
        /* Ask for notification when the server port has no more senders
         * A send-once right != a send right so our send-once right will
         * not interfere with the notification */
        err = mach_port_request_notification (mach_task_self (), g_service_port,
                                              MACH_NOTIFY_NO_SENDERS, true,
                                              g_notify_port,
                                              MACH_MSG_TYPE_MAKE_SEND_ONCE,
                                              &previous_notify_port);
    }

    if (!err) {
        /* Create the port set that the server will listen on */
        err = mach_port_allocate (mach_task_self (),
                                  MACH_PORT_RIGHT_PORT_SET, &g_listen_port_set);
    }

    if (!err) {
        /* Add the lookup port to the port set */
        err = mach_port_move_member (mach_task_self (),
                                     lookup_port, g_listen_port_set);
    }

    if (!err) {
        /* Add the service port to the port set */
        err = mach_port_move_member (mach_task_self (),
                                     g_service_port, g_listen_port_set);
    }

    if (!err) {
        /* Add the notify port to the port set */
        err = mach_port_move_member (mach_task_self (),
                                     g_notify_port, g_listen_port_set);
    }

    while (!err && !g_ready_to_quit) {
        /* Handle one message at a time so we can check to see if
         * the server wants to quit */
        err = mach_msg_server_once (k5_ipc_request_demux, K5_IPC_MAX_MSG_SIZE,
                                    g_listen_port_set, MACH_MSG_OPTION_NONE);
    }

    /* Clean up the ports and strings */
    if (MACH_PORT_VALID (g_notify_port)) {
        mach_port_destroy (mach_task_self (), g_notify_port);
        g_notify_port = MACH_PORT_NULL;
    }
    if (MACH_PORT_VALID (g_listen_port_set)) {
        mach_port_destroy (mach_task_self (), g_listen_port_set);
        g_listen_port_set = MACH_PORT_NULL;
    }
    if (MACH_PORT_VALID (boot_port)) {
        mach_port_deallocate (mach_task_self (), boot_port);
    }

    free (service);
    free (lookup);

    return err;
}

/* ------------------------------------------------------------------------ */

int32_t k5_ipc_server_send_reply (mach_port_t   in_reply_port,
                                  k5_ipc_stream in_reply_stream)
{
    kern_return_t err = KERN_SUCCESS;
    k5_ipc_inl_reply_t inl_reply;
    mach_msg_type_number_t inl_reply_length = 0;
    k5_ipc_ool_reply_t ool_reply = NULL;
    mach_msg_type_number_t ool_reply_length = 0;

    if (!MACH_PORT_VALID (in_reply_port)) { err = EINVAL; }
    if (!in_reply_stream                ) { err = EINVAL; }

    if (!err) {
        /* depending on how big the message is, use the fast inline buffer or
         * the slow dynamically allocated buffer */
        mach_msg_type_number_t reply_length = krb5int_ipc_stream_size (in_reply_stream);

        if (reply_length > K5_IPC_MAX_INL_MSG_SIZE) {
            //dprintf ("%s choosing out of line buffer (size is %d)",
            //                  __FUNCTION__, reply_length);

            err = vm_read (mach_task_self (),
                           (vm_address_t) krb5int_ipc_stream_data (in_reply_stream), reply_length,
                           (vm_address_t *) &ool_reply, &ool_reply_length);

        } else {
            //cci_debug_printf ("%s choosing in line buffer (size is %d)",
            //                  __FUNCTION__, reply_length);

            inl_reply_length = reply_length;
            memcpy (inl_reply, krb5int_ipc_stream_data (in_reply_stream), reply_length);
        }
    }

    if (!err) {
        err = k5_ipc_server_reply (in_reply_port,
                                   inl_reply, inl_reply_length,
                                   ool_reply, ool_reply_length);
    }

    if (!err) {
        /* Because we use ",dealloc" ool_reply will be freed by mach. Don't double free it. */
        ool_reply = NULL;
        ool_reply_length = 0;
    }

    if (ool_reply_length) { vm_deallocate (mach_task_self (), (vm_address_t) ool_reply, ool_reply_length); }

    return err;
}

/* ------------------------------------------------------------------------ */

void k5_ipc_server_quit (void)
{
    g_ready_to_quit = 1;
}
