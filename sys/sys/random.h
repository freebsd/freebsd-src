/*-
 * Copyright (c) 2000 Mark R. V. Murray
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
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
 * $FreeBSD: src/sys/sys/random.h,v 1.34 2004/10/12 14:55:59 rwatson Exp $
 */

#ifndef	_SYS_RANDOM_H_
#define	_SYS_RANDOM_H_

#ifdef _KERNEL

int read_random(void *, int);

/*
 * Note: if you add or remove members of esource, remember to also update the
 * KASSERT regarding what valid members are in random_harvest_internal().
 */
enum esource {
	RANDOM_START = 0,
	RANDOM_WRITE = 0,
	RANDOM_KEYBOARD,
	RANDOM_MOUSE,
	RANDOM_NET,
	RANDOM_INTERRUPT,
	RANDOM_PURE,
	ENTROPYSOURCE
};
void random_harvest(void *, u_int, u_int, u_int, enum esource);

/* Allow the sysadmin to select the broad category of
 * entropy types to harvest
 */
struct harvest_select {
	int ethernet;
	int point_to_point;
	int interrupt;
	int swi;
};

extern struct harvest_select harvest;

#endif /* _KERNEL */

#endif /* _SYS_RANDOM_H_ */
