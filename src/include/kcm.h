/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* include/kcm.h - Kerberos cache manager protocol declarations */
/*
 * Copyright (C) 2014 by the Massachusetts Institute of Technology.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef KCM_H
#define KCM_H

#define KCM_PROTOCOL_VERSION_MAJOR 2
#define KCM_PROTOCOL_VERSION_MINOR 0

#define KCM_UUID_LEN 16

/* This should ideally be in RUNSTATEDIR, but Heimdal uses a hardcoded
 * /var/run, and we need to use the same default path. */
#define DEFAULT_KCM_SOCKET_PATH "/var/run/.heim_org.h5l.kcm-socket"
#define DEFAULT_KCM_MACH_SERVICE "org.h5l.kcm"

/*
 * All requests begin with:
 *   major version (1 bytes)
 *   minor version (1 bytes)
 *   opcode (16-bit big-endian)
 *
 * All replies begin with a 32-bit big-endian reply code.
 *
 * Parameters are appended to the request or reply with no delimiters.  Flags
 * and time offsets are stored as 32-bit big-endian integers.  Names are
 * marshalled as zero-terminated strings.  Principals and credentials are
 * marshalled in the v4 FILE ccache format.  UUIDs are 16 bytes.  UUID lists
 * are not delimited, so nothing can come after them.
 */

/* Opcodes without comments are currently unused in the MIT client
 * implementation. */
typedef enum kcm_opcode {
    KCM_OP_NOOP,
    KCM_OP_GET_NAME,
    KCM_OP_RESOLVE,
    KCM_OP_GEN_NEW,             /*                     () -> (name)      */
    KCM_OP_INITIALIZE,          /*          (name, princ) -> ()          */
    KCM_OP_DESTROY,             /*                 (name) -> ()          */
    KCM_OP_STORE,               /*           (name, cred) -> ()          */
    KCM_OP_RETRIEVE,
    KCM_OP_GET_PRINCIPAL,       /*                 (name) -> (princ)     */
    KCM_OP_GET_CRED_UUID_LIST,  /*                 (name) -> (uuid, ...) */
    KCM_OP_GET_CRED_BY_UUID,    /*           (name, uuid) -> (cred)      */
    KCM_OP_REMOVE_CRED,         /* (name, flags, credtag) -> ()          */
    KCM_OP_SET_FLAGS,
    KCM_OP_CHOWN,
    KCM_OP_CHMOD,
    KCM_OP_GET_INITIAL_TICKET,
    KCM_OP_GET_TICKET,
    KCM_OP_MOVE_CACHE,
    KCM_OP_GET_CACHE_UUID_LIST, /*                     () -> (uuid, ...) */
    KCM_OP_GET_CACHE_BY_UUID,   /*                 (uuid) -> (name)      */
    KCM_OP_GET_DEFAULT_CACHE,   /*                     () -> (name)      */
    KCM_OP_SET_DEFAULT_CACHE,   /*                 (name) -> ()          */
    KCM_OP_GET_KDC_OFFSET,      /*                 (name) -> (offset)    */
    KCM_OP_SET_KDC_OFFSET,      /*         (name, offset) -> ()          */
    KCM_OP_ADD_NTLM_CRED,
    KCM_OP_HAVE_NTLM_CRED,
    KCM_OP_DEL_NTLM_CRED,
    KCM_OP_DO_NTLM_AUTH,
    KCM_OP_GET_NTLM_USER_LIST
} kcm_opcode;

#endif /* KCM_H */
