/*-
 * Copyright (c) 2002,2005 Marcel Moolenaar
 * Copyright (c) 2002 Hiten Mahesh Pandya
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

#ifndef _COMPAT_UUID_TO_STRING_H_
#define	_COMPAT_UUID_TO_STRING_H_

#include <stdio.h>

#define	OPENBSM_UUID_STR_LEN	37	/* 36 bytes including nul character. */

/*
 * uuid_to_string() - Convert a binary UUID into a string representation.
 * See also:
 *	http://www.opengroup.org/onlinepubs/009629399/uuid_to_string.htm
 *
 * NOTE: The references given above do not have a status code for when
 *	 the string could not be allocated. The status code has been
 *	 taken from the Hewlett-Packard implementation.
 */
static __inline int
openbsm_uuid_to_string(const void *u, char **s)
{
	const struct openbsm_uuid *op_uuidp;

	op_uuidp = u;

	*s = malloc(OPENBSM_UUID_STR_LEN);
	if (*s == NULL)
		return (-1);
	(void)snprintf(*s, OPENBSM_UUID_STR_LEN, 
	    "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
	    op_uuidp->time_low, op_uuidp->time_mid,
	    op_uuidp->time_hi_and_version,
	    op_uuidp->clock_seq_hi_and_reserved, op_uuidp->clock_seq_low,
	    op_uuidp->node[0], op_uuidp->node[1], op_uuidp->node[2],
	    op_uuidp->node[3], op_uuidp->node[4], op_uuidp->node[5]);
	return (0);
}

#endif /* !_COMPAT_UUID_TO_STRING_H_ */
