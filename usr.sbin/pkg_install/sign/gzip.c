/* $FreeBSD$ */
/* $OpenBSD: gzip.c,v 1.3 1999/10/04 21:46:28 espie Exp $ */
/*-
 * Copyright (c) 1999 Marc Espie.
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
 *	This product includes software developed by Marc Espie for the OpenBSD
 * Project.
 *
 * THIS SOFTWARE IS PROVIDED BY THE OPENBSD PROJECT AND CONTRIBUTORS 
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OPENBSD
 * PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "stand.h"
#include "gzip.h"
#include "pgp.h"

/*
 * Signatures follow a simple format
 * (endianess was chosen to conform to gzip header format)
 */

SIGNTAG known_tags[KNOWN_TAGS] = { 
	{'S', 'I', 'G', 'P', 'G', 'P', 0, 0 },
	{'C', 'K', 'S', 'H', 'A', '1', 0, 0 },
	{'C', 'R', 'X', '5', '0', '9', 0, 0 },
	{'S', 'i', 'g', 'P', 'G', 'P', 0, 0 }	/* old format */
};

void
sign_fill_tag(sign)
	struct signature *sign;
{
	sign->tag[6] = sign->length % 256;
	sign->tag[7] = sign->length / 256;
}
	
void
sign_fill_length(sign)
	struct signature *sign;
{
	sign->length = sign->tag[6] + 256 * sign->tag[7];
}

static size_t
stack_sign(match, t, f, sign)
	SIGNTAG match;
	int t;
	FILE *f;
	struct signature **sign;
{
	struct signature *new_sign;
	size_t length;
	
	new_sign = malloc(sizeof *new_sign);
	if (new_sign == NULL)
		return 0;
	new_sign->type = t;
	new_sign->next = NULL;
	memcpy(new_sign->tag, match, sizeof(SIGNTAG));	
	sign_fill_length(new_sign);
	new_sign->data = malloc(new_sign->length);
	if (new_sign->data == NULL || 
		fread(new_sign->data, 1, new_sign->length, f) != new_sign->length) {
		free_signature(new_sign);
		return 0;
	}
	length = new_sign->length;
	if (sign != NULL) {
		if (!*sign)
			*sign = new_sign;
		else {
			while ((*sign)->next != NULL)
				sign = &((*sign)->next);
			(*sign)->next = new_sign;
		}
	} else 
		free_signature(new_sign);
	return length;
}


static int 
add_sign(f, sign)
	FILE *f;
	struct signature **sign;
{
	SIGNTAG match;
	int i;

	if (fread(match, 1, sizeof(SIGNTAG), f) != sizeof(SIGNTAG)) 
		return -1;
	for (i = 0; i < KNOWN_TAGS; i++) {
		if (memcmp(match, known_tags[i], TAGCHECK) == 0) {
			unsigned int sign_length = stack_sign(match, i, f, sign);
			if (sign_length > 0)
				return sign_length + sizeof(SIGNTAG);
			else
				return -1;
		}
	}
	return 0;
}

static int
gzip_magic(f)
	FILE *f;
{
	int c, d;

	c = fgetc(f);
	d = fgetc(f);
	if ((unsigned char)c != (unsigned char)GZIP_MAGIC0 
		 || (unsigned char)d != (unsigned char)GZIP_MAGIC1)	
		return 0;
	else
		return 1;
}

static int
fill_gzip_fields(f, h)
	FILE *f;
	struct mygzip_header *h;
{
	int method, flags;
		
	method = fgetc(f);
	flags = fgetc(f);

	if (method == EOF || flags == EOF || fread(h->stamp, 1, 6, f) != 6)
		return 0;
	h->method = (char)method;
	h->flags = (char)flags;
	if ((h->flags & CONTINUATION) != 0)
		if (fread(h->part, 1, 2, f) != 2)
			return 0;
	return 1;
}

/* retrieve a gzip header, including signatures */
int 
gzip_read_header(f, h, sign)
	FILE *f;
	struct mygzip_header *h;
	struct signature **sign;
{
	if (sign != NULL)
		*sign = NULL;
	if (!gzip_magic(f) || !fill_gzip_fields(f, h))
		return GZIP_NOT_GZIP;

	if ((h->flags & EXTRA_FIELD) == 0) {
		h->remaining = 0;
		return GZIP_UNSIGNED;
	}
	else {
		int c;

		c = fgetc(f);
		if (c == EOF)
			return GZIP_NOT_GZIP;
		h->remaining = (unsigned)c;
		c = fgetc(f);
		if (c == EOF)
			return GZIP_NOT_PGPSIGNED;
		h->remaining += ((unsigned) c) << 8;
		while (h->remaining >= sizeof(SIGNTAG)) {
			int sign_length = add_sign(f, sign);
			if (sign_length > 0)
				h->remaining -= sign_length;
			if (sign_length < 0)
				return GZIP_NOT_GZIP;
			if (sign_length == 0)
				return GZIP_SIGNED;
		}
	return GZIP_SIGNED;
	}
}

static unsigned 
sign_length(sign)
	struct signature *sign;
{
	unsigned total = 0;

	while (sign != NULL)	{
		total += sizeof(SIGNTAG) + sign->length;
		sign = sign->next;
	}
	return total;
}

struct mydata {
	FILE *file;
	int ok;
};

static void myadd(arg, buffer, size)
	void *arg;
	const char *buffer;
	size_t size;
{
	struct mydata *d = arg;

	if (fwrite(buffer, 1, size, d->file) == size)
		d->ok = 1;
	else
		d->ok = 0;
}

/* write a gzip header, including signatures */
int 
gzip_write_header(f, h, sign)
	FILE *f;
	const struct mygzip_header *h;
	struct signature *sign;
{
	struct mydata d;
	d.file = f;
	if (gzip_copy_header(h, sign, myadd, &d) == 0)
		return 0;
	return d.ok;
}
		
int 
gzip_copy_header(h, sign, add, data)
	const struct mygzip_header *h;
	struct signature *sign;
	void (*add)(void *, const char *, size_t);
	void *data;
{
	char flags;
	size_t length;
	size_t buflength;
	size_t i;
	char *buffer;

	length = h->remaining + sign_length(sign);
	if (length) {
		buflength = length + 2;
		flags = h->flags | EXTRA_FIELD;
	} else {
		flags = h->flags & ~EXTRA_FIELD;
		buflength = 0;
	}
	buflength += 10;
	if ((h->flags & CONTINUATION) != 0)
		buflength += 2;

	buffer = malloc(buflength);
	if (buffer == NULL)
		return 0;

	i = 0;
	buffer[i++] = GZIP_MAGIC0;
	buffer[i++] = GZIP_MAGIC1;
	buffer[i++] = h->method;
	buffer[i++] = flags;
	memcpy(buffer+i, h->stamp, 6);
	i += 6;
	if ((flags & CONTINUATION) != 0) {
		memcpy(buffer+i, h->part, 2);
		i += 2;
	}
	if (length) {
		buffer[i++] = (char)(length % 256);
		buffer[i++] = (char)(length / 256);
		while (sign != NULL) {
			memcpy(buffer+i, sign->tag, sizeof(SIGNTAG));
			i += sizeof(SIGNTAG);
			memcpy(buffer+i, sign->data, sign->length);
			i += sign->length;
			sign = sign->next;
		}
	}
	(*add)(data, buffer, buflength);
	free(buffer);
	return 1;
}
	
void 
free_signature(sign)
	struct signature *sign;
{
	struct signature *next;

	while (sign != NULL) {
		next = sign->next;
		free(sign->data);
		free(sign);
		sign = next;
	}
}
