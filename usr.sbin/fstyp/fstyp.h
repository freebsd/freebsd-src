/*-
 * Copyright (c) 2014 The FreeBSD Foundation
 *
 * This software was developed by Edward Tomasz Napierala under sponsorship
 * from the FreeBSD Foundation.
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
 */

#ifndef FSTYP_H
#define	FSTYP_H

#include <stdbool.h>

#define	MIN(a,b) (((a)<(b))?(a):(b))

/* The spec doesn't seem to permit UTF-16 surrogates; definitely LE. */
#define	EXFAT_ENC	"UCS-2LE"
/*
 * NTFS itself is agnostic to encoding; it just stores 255 u16 wchars.  In
 * practice, UTF-16 seems expected for NTFS.  (Maybe also for exFAT.)
 */
#define	NTFS_ENC	"UTF-16LE"

extern bool	show_label;	/* -l flag */

void	*read_buf(FILE *fp, off_t off, size_t len);
char	*checked_strdup(const char *s);
void	rtrim(char *label, size_t size);

int	fstyp_apfs(FILE *fp, char *label, size_t size);
int	fstyp_befs(FILE *fp, char *label, size_t size);
int	fstyp_cd9660(FILE *fp, char *label, size_t size);
int	fstyp_exfat(FILE *fp, char *label, size_t size);
int	fstyp_ext2fs(FILE *fp, char *label, size_t size);
int	fstyp_geli(FILE *fp, char *label, size_t size);
int	fstyp_hammer(FILE *fp, char *label, size_t size);
int	fstyp_hammer2(FILE *fp, char *label, size_t size);
int	fstyp_hfsp(FILE *fp, char *label, size_t size);
int	fstyp_msdosfs(FILE *fp, char *label, size_t size);
int	fstyp_ntfs(FILE *fp, char *label, size_t size);
int	fstyp_ufs(FILE *fp, char *label, size_t size);
#ifdef HAVE_ZFS
int	fstyp_zfs(FILE *fp, char *label, size_t size);
#endif

#endif /* !FSTYP_H */
