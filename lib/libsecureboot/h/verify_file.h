/*-
 * Copyright (c) 2017-2018, Juniper Networks, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _VERIFY_FILE_H_
#define _VERIFY_FILE_H_

#define VE_GUESS        -1           /* let verify_file work it out */
#define VE_TRY          0            /* we don't mind if unverified */
#define VE_WANT         1            /* we want this verified */
#define VE_MUST         2            /* this must be verified */

#define VE_NOT_CHECKED -42
#define VE_VERIFIED     1            /* all good */
#define VE_UNVERIFIED_OK 0           /* not verified but that's ok */
#define VE_NOT_VERIFYING 2	     /* we are not verifying */

/* suitable buf size for hash_string */
#ifndef SHA_DIGEST_LENGTH
# define SHA_DIGEST_LENGTH 20
#endif

struct stat;

int	verify_prep(int, const char *, off_t, struct stat *, const char *);
void	ve_debug_set(int);
char	*ve_error_get(void);
void	ve_efi_init(void);
int	ve_status_get(int);
int	load_manifest(const char *, const char *, const char *, struct stat *);
int	pass_manifest(const char *, const char *);
int	pass_manifest_export_envs(void);
void	verify_report(const char *, int, int, struct stat *);
int	verify_file(int, const char *, off_t, int, const char *);
void	verify_pcr_export(void);
int	hash_string(char *s, size_t n, char *buf, size_t bufsz);
int	is_verified(struct stat *);
void	add_verify_status(struct stat *, int);

struct vectx;
struct vectx* vectx_open(int, const char *, off_t, struct stat *, int *, const char *);
ssize_t	vectx_read(struct vectx *, void *, size_t);
off_t	vectx_lseek(struct vectx *, off_t, int);
int	vectx_close(struct vectx *, int, const char *);

#endif	/* _VERIFY_FILE_H_ */
