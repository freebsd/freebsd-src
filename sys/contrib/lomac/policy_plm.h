/*-
 * Copyright (c) 2001 Networks Associates Technology, Inc.
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
 * $Id: policy_plm.h,v 1.20 2001/11/15 20:55:05 bfeldman Exp $
 */

#ifndef	LOMAC_PLM_H
#define	LOMAC_PLM_H

enum plm_level {
	LOW,
	SUBJ,
	HIGH
};
enum plm_flags {
	PLM_NOFLAGS, /* rule applies to this node and its children */
	PLM_CHILDOF  /* rule applies to node's children, not the node */
};
#define	LOWWRITE	LN_ATTR_LOWWRITE
#define	LOWNOOPEN	LN_ATTR_LOWNOOPEN
#define	NONETDEMOTE	LN_ATTR_NONETDEMOTE
#define	NODEMOTE	LN_ATTR_NODEMOTE

static u_int plm_levelflags_to_node_flags[3][2] = {
	{ LN_LOWEST_LEVEL,	LN_INHERIT_LOW },
	{ LN_SUBJ_LEVEL,	LN_INHERIT_SUBJ },
	{ LN_HIGHEST_LEVEL,	LN_INHERIT_HIGH }
};

typedef struct plm_rule {
	enum plm_level level;		/* LOMAC level */
	enum plm_flags flags;		/* flags for PLM evaluation */
	unsigned int attr;		/* LN_ATTR_MASK of flags */
	const char *path;		/* absolute path for this PLM rule */
} plm_rule_t;

/* The `plm' array maps levels onto all of the files in the filesystem */
static plm_rule_t plm[] = {
  { HIGH, PLM_NOFLAGS, 0, "/" },  /* everything initially inherits high level */
  { HIGH, PLM_CHILDOF, 0, "/" },
  { HIGH, PLM_NOFLAGS, NONETDEMOTE, "/sbin/dhclient" },
  { HIGH, PLM_CHILDOF, 0, "/var" },
  { HIGH, PLM_CHILDOF, LOWWRITE, "/dev" },
  { HIGH, PLM_NOFLAGS, LOWNOOPEN, "/dev/mdctl" },
  { HIGH, PLM_NOFLAGS, LOWNOOPEN, "/dev/pci" },
  { HIGH, PLM_NOFLAGS, LOWNOOPEN, "/dev/kmem" },
  { HIGH, PLM_NOFLAGS, LOWNOOPEN, "/dev/mem" },
  { HIGH, PLM_NOFLAGS, LOWNOOPEN, "/dev/io" },
  { HIGH, PLM_CHILDOF, 0, "/etc" },
  { HIGH, PLM_NOFLAGS, LOWWRITE, "/tmp" },
  { SUBJ, PLM_CHILDOF, 0, "/tmp" },
  { HIGH, PLM_NOFLAGS, 0, "/tmp/.X11-unix" },
  { HIGH, PLM_CHILDOF, LOWWRITE, "/tmp/.X11-unix" },
  { SUBJ, PLM_CHILDOF, 0, "/proc" },
  { LOW,  PLM_CHILDOF, 0, "/mnt" },  /* all nfs mounts are low */
  { LOW,  PLM_CHILDOF, 0, "/home" }, 
  { HIGH, PLM_NOFLAGS, NONETDEMOTE, "/usr/bin/env-nonetdemote" },
  { HIGH, PLM_NOFLAGS, NODEMOTE, "/usr/bin/env-nodemote" },
  { LOW,  PLM_CHILDOF, 0, "/usr/home" }, 
  { LOW,  PLM_CHILDOF, 0, "/var/lib" },
  { HIGH, PLM_NOFLAGS, LOWWRITE, "/var/tmp" },
  { SUBJ, PLM_CHILDOF, 0, "/var/tmp" },
  { LOW,  PLM_NOFLAGS, 0, "/var/tmp/vi.recover" },
  { SUBJ, PLM_CHILDOF, 0, "/var/tmp/vi.recover" },
  { HIGH, PLM_NOFLAGS, LOWWRITE, "/usr/tmp" },
  { SUBJ, PLM_CHILDOF, 0, "/usr/tmp" },
  { HIGH, PLM_NOFLAGS, 0, "/usr/tmp/.X11-unix" },
  { HIGH, PLM_CHILDOF, LOWWRITE, "/usr/tmp/.X11-unix" },
  { LOW,  PLM_NOFLAGS, 0, "/var/mail" },
  { LOW,  PLM_CHILDOF, 0, "/var/mail" },
  { LOW,  PLM_NOFLAGS, 0, "/var/spool/mqueue" },
  { LOW,  PLM_CHILDOF, 0, "/var/spool/mqueue" },
  { LOW,  PLM_NOFLAGS, 0, "/dev/log" },
  { HIGH, PLM_NOFLAGS, 0, "/home/ftp" },
  { HIGH, PLM_NOFLAGS, 0, "/usr/home/ftp" },
  { HIGH, PLM_NOFLAGS, 0, "/mnt/cdrom" },  /* cdrom is high */
  { HIGH, PLM_NOFLAGS, 0, "/home/samba" },
  { HIGH, PLM_NOFLAGS, 0, "/usr/home/samba" },
  { LOW,  PLM_NOFLAGS, 0, "/dev/printer" },
  { HIGH, PLM_CHILDOF, 0, "/var/log" },
  { LOW,  PLM_NOFLAGS, 0, "/var/log/sendmail.st" },
  { HIGH, PLM_NOFLAGS, LOWWRITE, "/var/run/utmp" },
  { HIGH, PLM_NOFLAGS, LOWWRITE, "/var/log/lastlog" },
  { HIGH, PLM_NOFLAGS, LOWWRITE, "/var/log/wtmp" },
  { 0, 0, 0 }
};

#endif /* LOMAC_PLM_H */
