/*-
 * Copyright (c) 1999, 2000 Andrew J. Korty
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 *
 */

#define SSH_CLIENT_DIR		".ssh"
#define SSH_CLIENT_IDENTITY	"identity"
#define SSH_CLIENT_ID_DSA	"id_dsa"

/*
 * Compatibility with SSH2 from SSH Communications Security.
 */

#define SSH2_CLIENT_DIR		".ssh2"
#define SSH2_DSA_PREFIX		"id_dsa_"
#define SSH2_PUB_SUFFIX		".pub"
#define SSH2_RSA_PREFIX		"id_rsa_"

#define	MODULE_NAME	"pam_ssh"
#define	NEED_PASSPHRASE	"SSH passphrase: "
#define	SSH_AGENT	"ssh-agent"

#define ENV_PID_SUFFIX		"_AGENT_PID"
#define ENV_SOCKET_SUFFIX	"_AUTH_SOCK"
