/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (C) 1995, 1996, 1997 Wolfgang Solfrank
 * Copyright (c) 1995 Martin Husemann
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *	$NetBSD: ext.h,v 1.6 2000/04/25 23:02:51 jdolecek Exp $
 * $FreeBSD$
 */

#ifndef EXT_H
#define	EXT_H

#include <sys/types.h>

#include <stdbool.h>

#include "dosfs.h"

#define	LOSTDIR	"LOST.DIR"

/*
 * Options:
 */
extern int alwaysno;	/* assume "no" for all questions */
extern int alwaysyes;	/* assume "yes" for all questions */
extern int preen;	/* we are preening */
extern int rdonly;	/* device is opened read only (supersedes above) */
extern int skipclean;	/* skip clean file systems if preening */
extern int allow_mmap;  /* allow the use of mmap() */

/*
 * function declarations
 */
int ask(int, const char *, ...) __printflike(2, 3);

/*
 * Check the dirty flag.  If the file system is clean, then return 1.
 * Otherwise, return 0 (this includes the case of FAT12 file systems --
 * they have no dirty flag, so they must be assumed to be unclean).
 */
int checkdirty(int, struct bootblock *);

/*
 * Check file system given as arg
 */
int checkfilesys(const char *);

/*
 * Return values of various functions
 */
#define	FSOK		0		/* Check was OK */
#define	FSBOOTMOD	1		/* Boot block was modified */
#define	FSDIRMOD	2		/* Some directory was modified */
#define	FSFATMOD	4		/* The FAT was modified */
#define	FSERROR		8		/* Some unrecovered error remains */
#define	FSFATAL		16		/* Some unrecoverable error occurred */
#define	FSDIRTY		32		/* File system is dirty */

/*
 * read a boot block in a machine independent fashion and translate
 * it into our struct bootblock.
 */
int readboot(int, struct bootblock *);

/*
 * Correct the FSInfo block.
 */
int writefsinfo(int, struct bootblock *);

/* Opaque type */
struct fat_descriptor;

void fat_clear_cl_head(struct fat_descriptor *, cl_t);
bool fat_is_cl_head(struct fat_descriptor *, cl_t);

cl_t fat_get_cl_next(struct fat_descriptor *, cl_t);

int fat_set_cl_next(struct fat_descriptor *, cl_t, cl_t);

cl_t fat_allocate_cluster(struct fat_descriptor *fat);

struct bootblock* fat_get_boot(struct fat_descriptor *);
int fat_get_fd(struct fat_descriptor *);
bool fat_is_valid_cl(struct fat_descriptor *, cl_t);

/*
 * Read the FAT 0 and return a pointer to the newly allocated
 * descriptor of it.
 */
int readfat(int, struct bootblock *, struct fat_descriptor **);

/*
 * Write back FAT entries
 */
int writefat(struct fat_descriptor *);

/*
 * Read a directory
 */
int resetDosDirSection(struct fat_descriptor *);
void finishDosDirSection(void);
int handleDirTree(struct fat_descriptor *);

/*
 * Cross-check routines run after everything is completely in memory
 */
int checkchain(struct fat_descriptor *, cl_t, size_t *);

/*
 * Check for lost cluster chains
 */
int checklost(struct fat_descriptor *);
/*
 * Try to reconnect a lost cluster chain
 */
int reconnect(struct fat_descriptor *, cl_t, size_t);
void finishlf(void);

/*
 * Small helper functions
 */
/*
 * Return the type of a reserved cluster as text
 */
const char *rsrvdcltype(cl_t);

/*
 * Clear a cluster chain in a FAT
 */
void clearchain(struct fat_descriptor *, cl_t);

#endif
