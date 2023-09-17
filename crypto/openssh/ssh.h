/* $OpenBSD: ssh.h,v 1.90 2020/07/14 23:57:01 djm Exp $ */

/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 */

/* Cipher used for encrypting authentication files. */
#define SSH_AUTHFILE_CIPHER	SSH_CIPHER_3DES

/* Default port number. */
#define SSH_DEFAULT_PORT	22

/*
 * Maximum number of certificate files that can be specified
 * in configuration files or on the command line.
 */
#define SSH_MAX_CERTIFICATE_FILES	100

/*
 * Maximum number of RSA authentication identity files that can be specified
 * in configuration files or on the command line.
 */
#define SSH_MAX_IDENTITY_FILES		100

/*
 * Major protocol version.  Different version indicates major incompatibility
 * that prevents communication.
 *
 * Minor protocol version.  Different version indicates minor incompatibility
 * that does not prevent interoperation.
 */
#define PROTOCOL_MAJOR_1	1
#define PROTOCOL_MINOR_1	5

/* We support only SSH2 */
#define PROTOCOL_MAJOR_2	2
#define PROTOCOL_MINOR_2	0

/*
 * Name for the service.  The port named by this service overrides the
 * default port if present.
 */
#define SSH_SERVICE_NAME	"ssh"

/*
 * Name of the environment variable containing the process ID of the
 * authentication agent.
 */
#define SSH_AGENTPID_ENV_NAME	"SSH_AGENT_PID"

/*
 * Name of the environment variable containing the pathname of the
 * authentication socket.
 */
#define SSH_AUTHSOCKET_ENV_NAME "SSH_AUTH_SOCK"

/*
 * Environment variable for overwriting the default location of askpass
 */
#define SSH_ASKPASS_ENV		"SSH_ASKPASS"

/*
 * Environment variable to control whether or not askpass is used.
 */
#define SSH_ASKPASS_REQUIRE_ENV		"SSH_ASKPASS_REQUIRE"

/*
 * Force host key length and server key length to differ by at least this
 * many bits.  This is to make double encryption with rsaref work.
 */
#define SSH_KEY_BITS_RESERVED		128

/*
 * Length of the session key in bytes.  (Specified as 256 bits in the
 * protocol.)
 */
#define SSH_SESSION_KEY_LENGTH		32

/* Used to identify ``EscapeChar none'' */
#define SSH_ESCAPECHAR_NONE		-2

/*
 * unprivileged user when UsePrivilegeSeparation=yes;
 * sshd will change its privileges to this user and its
 * primary group.
 */
#ifndef SSH_PRIVSEP_USER
#define SSH_PRIVSEP_USER		"sshd"
#endif

/* Listen backlog for sshd, ssh-agent and forwarding sockets */
#define SSH_LISTEN_BACKLOG		128

/* Limits for banner exchange */
#define SSH_MAX_BANNER_LEN		8192
#define SSH_MAX_PRE_BANNER_LINES	1024
