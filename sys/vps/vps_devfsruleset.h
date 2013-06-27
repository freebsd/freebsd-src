/*-
 * Copyright (c) 2009-2013 Klaus P. Ohrhallinger <k@7he.at>
 * All rights reserved.
 *
 * Development of this software was partly funded by:
 *    TransIP.nl <http://www.transip.nl/>
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
 * OR SERVICES, LOSS OF USE, DATA, OR PROFITS, OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* $Id: vps_devfsruleset.h 126 2013-04-07 15:55:54Z klaus $ */

#ifndef _VPS_DEVFSRULESET_H
#define _VPS_DEVFSRULESET_H

#include <sys/cdefs.h>

struct devfs_rule vps_devfs_ruleset_default[] = {
	/* Hide all. */
	{
	.dr_magic = DEVFS_MAGIC,
	.dr_id = mkrid(100, 100),
	.dr_icond = 0,
	.dr_iacts = DRA_BACTS,
	.dr_bacts = DRB_HIDE,
	},
	/* Unhide some things. */
	{
	.dr_magic = DEVFS_MAGIC,
	.dr_id = mkrid(100, 200),
	.dr_icond = DRC_PATHPTRN,
	.dr_iacts = DRA_BACTS,
	.dr_bacts = DRB_UNHIDE,
	.dr_pathptrn = "null",
	},
	{
	.dr_magic = DEVFS_MAGIC,
	.dr_id = mkrid(100, 201),
	.dr_icond = DRC_PATHPTRN,
	.dr_iacts = DRA_BACTS,
	.dr_bacts = DRB_UNHIDE,
	.dr_pathptrn = "zero",
	},
	{
	.dr_magic = DEVFS_MAGIC,
	.dr_id = mkrid(100, 202),
	.dr_icond = DRC_PATHPTRN,
	.dr_iacts = DRA_BACTS,
	.dr_bacts = DRB_UNHIDE,
	.dr_pathptrn = "crypto",
	},
	{
	.dr_magic = DEVFS_MAGIC,
	.dr_id = mkrid(100, 203),
	.dr_icond = DRC_PATHPTRN,
	.dr_iacts = DRA_BACTS,
	.dr_bacts = DRB_UNHIDE,
	.dr_pathptrn = "random",
	},
	{
	.dr_magic = DEVFS_MAGIC,
	.dr_id = mkrid(100, 204),
	.dr_icond = DRC_PATHPTRN,
	.dr_iacts = DRA_BACTS,
	.dr_bacts = DRB_UNHIDE,
	.dr_pathptrn = "urandom",
	},
	{
	.dr_magic = DEVFS_MAGIC,
	.dr_id = mkrid(100, 205),
	.dr_icond = DRC_PATHPTRN,
	.dr_iacts = DRA_BACTS,
	.dr_bacts = DRB_UNHIDE,
	.dr_pathptrn = "console",
	},
	{
	.dr_magic = DEVFS_MAGIC,
	.dr_id = mkrid(100, 300),
	.dr_icond = DRC_PATHPTRN,
	.dr_iacts = DRA_BACTS,
	.dr_bacts = DRB_UNHIDE,
	.dr_pathptrn = "ptyp*",
	},
	{
	.dr_magic = DEVFS_MAGIC,
	.dr_id = mkrid(100, 301),
	.dr_icond = DRC_PATHPTRN,
	.dr_iacts = DRA_BACTS,
	.dr_bacts = DRB_UNHIDE,
	.dr_pathptrn = "ptyq*",
	},
	{
	.dr_magic = DEVFS_MAGIC,
	.dr_id = mkrid(100, 302),
	.dr_icond = DRC_PATHPTRN,
	.dr_iacts = DRA_BACTS,
	.dr_bacts = DRB_UNHIDE,
	.dr_pathptrn = "ptyr*",
	},
	{
	.dr_magic = DEVFS_MAGIC,
	.dr_id = mkrid(100, 303),
	.dr_icond = DRC_PATHPTRN,
	.dr_iacts = DRA_BACTS,
	.dr_bacts = DRB_UNHIDE,
	.dr_pathptrn = "ptys*",
	},
	{
	.dr_magic = DEVFS_MAGIC,
	.dr_id = mkrid(100, 304),
	.dr_icond = DRC_PATHPTRN,
	.dr_iacts = DRA_BACTS,
	.dr_bacts = DRB_UNHIDE,
	.dr_pathptrn = "ptyP*",
	},
	{
	.dr_magic = DEVFS_MAGIC,
	.dr_id = mkrid(100, 305),
	.dr_icond = DRC_PATHPTRN,
	.dr_iacts = DRA_BACTS,
	.dr_bacts = DRB_UNHIDE,
	.dr_pathptrn = "ptyQ*",
	},
	{
	.dr_magic = DEVFS_MAGIC,
	.dr_id = mkrid(100, 306),
	.dr_icond = DRC_PATHPTRN,
	.dr_iacts = DRA_BACTS,
	.dr_bacts = DRB_UNHIDE,
	.dr_pathptrn = "ptyR*",
	},
	{
	.dr_magic = DEVFS_MAGIC,
	.dr_id = mkrid(100, 307),
	.dr_icond = DRC_PATHPTRN,
	.dr_iacts = DRA_BACTS,
	.dr_bacts = DRB_UNHIDE,
	.dr_pathptrn = "ptyS*",
	},
	{
	.dr_magic = DEVFS_MAGIC,
	.dr_id = mkrid(100, 308),
	.dr_icond = DRC_PATHPTRN,
	.dr_iacts = DRA_BACTS,
	.dr_bacts = DRB_UNHIDE,
	.dr_pathptrn = "ttyp*",
	},
	{
	.dr_magic = DEVFS_MAGIC,
	.dr_id = mkrid(100, 309),
	.dr_icond = DRC_PATHPTRN,
	.dr_iacts = DRA_BACTS,
	.dr_bacts = DRB_UNHIDE,
	.dr_pathptrn = "ttyq*",
	},
	{
	.dr_magic = DEVFS_MAGIC,
	.dr_id = mkrid(100, 310),
	.dr_icond = DRC_PATHPTRN,
	.dr_iacts = DRA_BACTS,
	.dr_bacts = DRB_UNHIDE,
	.dr_pathptrn = "ttyr*",
	},
	{
	.dr_magic = DEVFS_MAGIC,
	.dr_id = mkrid(100, 311),
	.dr_icond = DRC_PATHPTRN,
	.dr_iacts = DRA_BACTS,
	.dr_bacts = DRB_UNHIDE,
	.dr_pathptrn = "ttys*",
	},
	{
	.dr_magic = DEVFS_MAGIC,
	.dr_id = mkrid(100, 312),
	.dr_icond = DRC_PATHPTRN,
	.dr_iacts = DRA_BACTS,
	.dr_bacts = DRB_UNHIDE,
	.dr_pathptrn = "ttyP*",
	},
	{
	.dr_magic = DEVFS_MAGIC,
	.dr_id = mkrid(100, 313),
	.dr_icond = DRC_PATHPTRN,
	.dr_iacts = DRA_BACTS,
	.dr_bacts = DRB_UNHIDE,
	.dr_pathptrn = "ttyQ*",
	},
	{
	.dr_magic = DEVFS_MAGIC,
	.dr_id = mkrid(100, 314),
	.dr_icond = DRC_PATHPTRN,
	.dr_iacts = DRA_BACTS,
	.dr_bacts = DRB_UNHIDE,
	.dr_pathptrn = "ttyR*",
	},
	{
	.dr_magic = DEVFS_MAGIC,
	.dr_id = mkrid(100, 315),
	.dr_icond = DRC_PATHPTRN,
	.dr_iacts = DRA_BACTS,
	.dr_bacts = DRB_UNHIDE,
	.dr_pathptrn = "ttyS*",
	},
	{
	.dr_magic = DEVFS_MAGIC,
	.dr_id = mkrid(100, 316),
	.dr_icond = DRC_PATHPTRN,
	.dr_iacts = DRA_BACTS,
	.dr_bacts = DRB_UNHIDE,
	.dr_pathptrn = "pts",
	},
	{
	.dr_magic = DEVFS_MAGIC,
	.dr_id = mkrid(100, 317),
	.dr_icond = DRC_PATHPTRN,
	.dr_iacts = DRA_BACTS,
	.dr_bacts = DRB_UNHIDE,
	.dr_pathptrn = "pts/*",
	},
	{
	.dr_magic = DEVFS_MAGIC,
	.dr_id = mkrid(100, 318),
	.dr_icond = DRC_PATHPTRN,
	.dr_iacts = DRA_BACTS,
	.dr_bacts = DRB_UNHIDE,
	.dr_pathptrn = "pty/*",
	},
	{
	.dr_magic = DEVFS_MAGIC,
	.dr_id = mkrid(100, 319),
	.dr_icond = DRC_PATHPTRN,
	.dr_iacts = DRA_BACTS,
	.dr_bacts = DRB_UNHIDE,
	.dr_pathptrn = "fd",
	},
	{
	.dr_magic = DEVFS_MAGIC,
	.dr_id = mkrid(100, 320),
	.dr_icond = DRC_PATHPTRN,
	.dr_iacts = DRA_BACTS,
	.dr_bacts = DRB_UNHIDE,
	.dr_pathptrn = "fd/*",
	},
	{
	.dr_magic = DEVFS_MAGIC,
	.dr_id = mkrid(100, 321),
	.dr_icond = DRC_PATHPTRN,
	.dr_iacts = DRA_BACTS,
	.dr_bacts = DRB_UNHIDE,
	.dr_pathptrn = "stdin",
	},
	{
	.dr_magic = DEVFS_MAGIC,
	.dr_id = mkrid(100, 322),
	.dr_icond = DRC_PATHPTRN,
	.dr_iacts = DRA_BACTS,
	.dr_bacts = DRB_UNHIDE,
	.dr_pathptrn = "stdout",
	},
	{
	.dr_magic = DEVFS_MAGIC,
	.dr_id = mkrid(100, 323),
	.dr_icond = DRC_PATHPTRN,
	.dr_iacts = DRA_BACTS,
	.dr_bacts = DRB_UNHIDE,
	.dr_pathptrn = "stderr",
	},
	{
	.dr_magic = DEVFS_MAGIC,
	.dr_id = mkrid(100, 324),
	.dr_icond = DRC_PATHPTRN,
	.dr_iacts = DRA_BACTS,
	.dr_bacts = DRB_UNHIDE,
	.dr_pathptrn = "ttyv*",
	},
};

#endif /* _VPS_DEVFSRULESET_H */

/* EOF */
