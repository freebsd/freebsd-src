/*-
 * Copyright (c) 2009-2010 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Pawel Jakub Dawidek under sponsorship from
 * the FreeBSD Foundation.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef	_HAST_H_
#define	_HAST_H_

#include <sys/queue.h>
#include <sys/socket.h>

#include <arpa/inet.h>

#include <netinet/in.h>

#include <limits.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

#include <activemap.h>

#include "proto.h"

/*
 * Version history:
 * 0 - initial version
 * 1 - HIO_KEEPALIVE added
 */
#define	HAST_PROTO_VERSION	1

#define	EHAST_OK		0
#define	EHAST_NOENTRY		1
#define	EHAST_INVALID		2
#define	EHAST_NOMEMORY		3
#define	EHAST_UNIMPLEMENTED	4

#define	HASTCTL_CMD_UNKNOWN	0
#define	HASTCTL_CMD_SETROLE	1
#define	HASTCTL_CMD_STATUS	2

#define	HAST_ROLE_UNDEF		0
#define	HAST_ROLE_INIT		1
#define	HAST_ROLE_PRIMARY	2
#define	HAST_ROLE_SECONDARY	3

#define	HAST_SYNCSRC_UNDEF	0
#define	HAST_SYNCSRC_PRIMARY	1
#define	HAST_SYNCSRC_SECONDARY	2

#define	HIO_UNDEF		0
#define	HIO_READ		1
#define	HIO_WRITE		2
#define	HIO_DELETE		3
#define	HIO_FLUSH		4
#define	HIO_KEEPALIVE		5

#define	HAST_USER	"hast"
#define	HAST_TIMEOUT	5
#define	HAST_CONFIG	"/etc/hast.conf"
#define	HAST_CONTROL	"/var/run/hastctl"
#define	HASTD_PORT	8457
#define	HASTD_LISTEN	"tcp4://0.0.0.0:8457"
#define	HASTD_PIDFILE	"/var/run/hastd.pid"

/* Default extent size. */
#define	HAST_EXTENTSIZE	2097152
/* Default maximum number of extents that are kept dirty. */
#define	HAST_KEEPDIRTY	64

#define	HAST_ADDRSIZE	1024
#define	HAST_TOKEN_SIZE	16

struct hastd_config {
	/* Address to communicate with hastctl(8). */
	char	 hc_controladdr[HAST_ADDRSIZE];
	/* Protocol-specific data. */
	struct proto_conn *hc_controlconn;
	/* Incoming control connection. */
	struct proto_conn *hc_controlin;
	/* Address to listen on. */
	char	 hc_listenaddr[HAST_ADDRSIZE];
	/* Protocol-specific data. */
	struct proto_conn *hc_listenconn;
	/* List of resources. */
	TAILQ_HEAD(, hast_resource) hc_resources;
};

#define	HAST_REPLICATION_FULLSYNC	0
#define	HAST_REPLICATION_MEMSYNC	1
#define	HAST_REPLICATION_ASYNC		2

/*
 * Structure that describes single resource.
 */
struct hast_resource {
	/* Resource name. */
	char	hr_name[NAME_MAX];
	/* Replication mode (HAST_REPLICATION_*). */
	int	hr_replication;
	/* Provider name that will appear in /dev/hast/. */
	char	hr_provname[NAME_MAX];
	/* Synchronization extent size. */
	int	hr_extentsize;
	/* Maximum number of extents that are kept dirty. */
	int	hr_keepdirty;
	/* Path to a program to execute on various events. */
	char	hr_exec[PATH_MAX];

	/* Path to local component. */
	char	hr_localpath[PATH_MAX];
	/* Descriptor to access local component. */
	int	hr_localfd;
	/* Offset into local component. */
	off_t	hr_localoff;
	/* Size of usable space. */
	off_t	hr_datasize;
	/* Size of entire local provider. */
	off_t	hr_local_mediasize;
	/* Sector size of local provider. */
	unsigned int hr_local_sectorsize;

	/* Descriptor for /dev/ggctl communication. */
	int	hr_ggatefd;
	/* Unit number for ggate communication. */
	int	hr_ggateunit;

	/* Address of the remote component. */
	char	hr_remoteaddr[HAST_ADDRSIZE];
	/* Connection for incoming data. */
	struct proto_conn *hr_remotein;
	/* Connection for outgoing data. */
	struct proto_conn *hr_remoteout;
	/* Token to verify both in and out connection are coming from
	   the same node (not necessarily from the same address). */
	unsigned char hr_token[HAST_TOKEN_SIZE];
	/* Connection timeout. */
	int	hr_timeout;

	/* Resource unique identifier. */
	uint64_t hr_resuid;
	/* Primary's local modification count. */
	uint64_t hr_primary_localcnt;
	/* Primary's remote modification count. */
	uint64_t hr_primary_remotecnt;
	/* Secondary's local modification count. */
	uint64_t hr_secondary_localcnt;
	/* Secondary's remote modification count. */
	uint64_t hr_secondary_remotecnt;
	/* Synchronization source. */
	uint8_t hr_syncsrc;

	/* Resource role: HAST_ROLE_{INIT,PRIMARY,SECONDARY}. */
	int	hr_role;
	/* Previous resource role: HAST_ROLE_{INIT,PRIMARY,SECONDARY}. */
	int	hr_previous_role;
	/* PID of child worker process. 0 - no child. */
	pid_t	hr_workerpid;
	/* Control commands from parent to child. */
	struct proto_conn *hr_ctrl;
	/* Events from child to parent. */
	struct proto_conn *hr_event;
	/* Connection requests from child to parent. */
	struct proto_conn *hr_conn;

	/* Activemap structure. */
	struct activemap *hr_amp;
	/* Locked used to synchronize access to hr_amp. */
	pthread_mutex_t hr_amp_lock;

	/* Next resource. */
	TAILQ_ENTRY(hast_resource) hr_next;
};

struct hastd_config *yy_config_parse(const char *config, bool exitonerror);
void yy_config_free(struct hastd_config *config);

void yyerror(const char *);
int yylex(void);
int yyparse(void);

#endif	/* !_HAST_H_ */
