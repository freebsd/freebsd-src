/*
 * Copyright (c) 1990 Jan-Simon Pendry
 * Copyright (c) 1990 Imperial College of Science, Technology & Medicine
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry at Imperial College, London.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)amq.h	8.1 (Berkeley) 6/6/93
 *
 * $Id: amq.h,v 1.1.1.1 1994/05/26 05:22:05 rgrimes Exp $
 *
 */

#define AMQ_STRLEN 1024

typedef char *amq_string;
bool_t xdr_amq_string();


typedef long time_type;
bool_t xdr_time_type();


struct amq_mount_tree {
	amq_string mt_mountinfo;
	amq_string mt_directory;
	amq_string mt_mountpoint;
	amq_string mt_type;
	time_type mt_mounttime;
	u_short mt_mountuid;
	int mt_getattr;
	int mt_lookup;
	int mt_readdir;
	int mt_readlink;
	int mt_statfs;
	struct amq_mount_tree *mt_next;
	struct amq_mount_tree *mt_child;
};
typedef struct amq_mount_tree amq_mount_tree;
bool_t xdr_amq_mount_tree();


typedef amq_mount_tree *amq_mount_tree_p;
bool_t xdr_amq_mount_tree_p();


struct amq_mount_info {
	amq_string mi_type;
	amq_string mi_mountpt;
	amq_string mi_mountinfo;
	amq_string mi_fserver;
	int mi_error;
	int mi_refc;
	int mi_up;
};
typedef struct amq_mount_info amq_mount_info;
bool_t xdr_amq_mount_info();


typedef struct {
	u_int amq_mount_info_list_len;
	amq_mount_info *amq_mount_info_list_val;
} amq_mount_info_list;
bool_t xdr_amq_mount_info_list();


typedef struct {
	u_int amq_mount_tree_list_len;
	amq_mount_tree_p *amq_mount_tree_list_val;
} amq_mount_tree_list;
bool_t xdr_amq_mount_tree_list();


struct amq_mount_stats {
	int as_drops;
	int as_stale;
	int as_mok;
	int as_merr;
	int as_uerr;
};
typedef struct amq_mount_stats amq_mount_stats;
bool_t xdr_amq_mount_stats();


enum amq_opt {
	AMOPT_DEBUG = 0,
	AMOPT_LOGFILE = 1,
	AMOPT_XLOG = 2,
	AMOPT_FLUSHMAPC = 3
};
typedef enum amq_opt amq_opt;
bool_t xdr_amq_opt();


struct amq_setopt {
	amq_opt as_opt;
	amq_string as_str;
};
typedef struct amq_setopt amq_setopt;
bool_t xdr_amq_setopt();


#define AMQ_PROGRAM ((u_long)300019)
#define AMQ_VERSION ((u_long)1)
#define AMQPROC_NULL ((u_long)0)
extern voidp amqproc_null_1();
#define AMQPROC_MNTTREE ((u_long)1)
extern amq_mount_tree_p *amqproc_mnttree_1();
#define AMQPROC_UMNT ((u_long)2)
extern voidp amqproc_umnt_1();
#define AMQPROC_STATS ((u_long)3)
extern amq_mount_stats *amqproc_stats_1();
#define AMQPROC_EXPORT ((u_long)4)
extern amq_mount_tree_list *amqproc_export_1();
#define AMQPROC_SETOPT ((u_long)5)
extern int *amqproc_setopt_1();
#define AMQPROC_GETMNTFS ((u_long)6)
extern amq_mount_info_list *amqproc_getmntfs_1();
#define AMQPROC_MOUNT ((u_long)7)
extern int *amqproc_mount_1();
#define AMQPROC_GETVERS ((u_long)8)
extern amq_string *amqproc_getvers_1();

