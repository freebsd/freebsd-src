/*-
 * Copyright (c) 2003 TAKAHASHI Yoshihiro
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/diskpc98.h>
#include <sys/endian.h>

void
pc98_partition_dec(void const *pp, struct pc98_partition *d)
{
	unsigned char const *ptr = pp;
	u_int i;

	d->dp_mid = ptr[0];
	d->dp_sid = ptr[1];
	d->dp_dum1 = ptr[2];
	d->dp_dum2 = ptr[3];
	d->dp_ipl_sct = ptr[4];
	d->dp_ipl_head = ptr[5];
	d->dp_ipl_cyl = le16dec(ptr + 6);
	d->dp_ssect = ptr[8];
	d->dp_shd = ptr[9];
	d->dp_scyl = le16dec(ptr + 10);
	d->dp_esect = ptr[12];
	d->dp_ehd = ptr[13];
	d->dp_ecyl = le16dec(ptr + 14);
	for (i = 0; i < sizeof (d->dp_name); i++)
		d->dp_name[i] = ptr[16 + i];
}

void
pc98_partition_enc(void *pp, struct pc98_partition *d)
{
	unsigned char *ptr = pp;
	u_int i;

	ptr[0] = d->dp_mid;
	ptr[1] = d->dp_sid;
	ptr[2] = d->dp_dum1;
	ptr[3] = d->dp_dum2;
	ptr[4] = d->dp_ipl_sct;
	ptr[5] = d->dp_ipl_head;
	le16enc(ptr + 6, d->dp_ipl_cyl);
	ptr[8] = d->dp_ssect;
	ptr[9] = d->dp_shd;
	le16enc(ptr + 10, d->dp_scyl);
	ptr[12] = d->dp_esect;
	ptr[13] = d->dp_ehd;
	le16enc(ptr + 14, d->dp_ecyl);
	for (i = 0; i < sizeof (d->dp_name); i++)
		ptr[16 + i] = d->dp_name[i];
}
