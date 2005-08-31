/*-
 * Copyright (c) 2002 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _GPT_H_
#define	_GPT_H_

#include <sys/endian.h>
#include <sys/gpt.h>

#include <uuid.h>

void	le_uuid_dec(void const *, uuid_t *);
void	le_uuid_enc(void *, uuid_t const *);

struct mbr_part {
	uint8_t		part_flag;		/* bootstrap flags */
	uint8_t		part_shd;		/* starting head */
	uint8_t		part_ssect;		/* starting sector */
	uint8_t		part_scyl;		/* starting cylinder */
	uint8_t		part_typ;		/* partition type */
	uint8_t		part_ehd;		/* end head */
	uint8_t		part_esect;		/* end sector */
	uint8_t		part_ecyl;		/* end cylinder */
	uint16_t	part_start_lo;		/* absolute starting ... */
	uint16_t	part_start_hi;		/* ... sector number */
	uint16_t	part_size_lo;		/* partition size ... */
	uint16_t	part_size_hi;		/* ... in sectors */
};

struct mbr {
	uint16_t	mbr_code[223];
	struct mbr_part	mbr_part[4];
	uint16_t	mbr_sig;
#define	MBR_SIG		0xAA55
};

extern char *device_name;
extern off_t mediasz;
extern u_int parts;
extern u_int secsz;
extern int readonly, verbose;

uint32_t crc32(const void *, size_t);
void	gpt_close(int);
int	gpt_open(const char *);
void*	gpt_read(int, off_t, size_t);
int	gpt_write(int, map_t *);
void	unicode16(short *, const wchar_t *, size_t);

int	cmd_add(int, char *[]);
int	cmd_create(int, char *[]);
int	cmd_destroy(int, char *[]);
int	cmd_migrate(int, char *[]);
int	cmd_recover(int, char *[]);
int	cmd_remove(int, char *[]);
int	cmd_show(int, char *[]);

#endif /* _GPT_H_ */
