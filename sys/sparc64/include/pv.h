/*-
 * Copyright (c) 1991 Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department and William Jolitz of UUNET Technologies Inc.
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
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef	_MACHINE_PV_H_
#define	_MACHINE_PV_H_

extern uma_zone_t pvzone;
extern struct vm_object pvzone_obj;
extern int pv_entry_count;
extern int pv_entry_max;
extern int pv_entry_high_water;
extern struct pv_entry *pvinit;

void *pv_allocf(uma_zone_t zone, int bytes, u_int8_t *flags, int wait);
pv_entry_t pv_alloc(void);
void pv_free(pv_entry_t pv);

void pv_insert(pmap_t pm, vm_page_t m, vm_offset_t va);
pv_entry_t pv_lookup(pmap_t pm, vm_page_t m, vm_offset_t va);
void pv_remove(pmap_t pm, vm_page_t m, vm_offset_t va);
int pv_page_exists(pmap_t pm, vm_page_t m);
void pv_remove_all(vm_page_t m);

void pv_bit_clear(vm_page_t m, u_long bits);
int pv_bit_count(vm_page_t m, u_long bits);
int pv_bit_test(vm_page_t m, u_long bits);

#endif /* !_MACHINE_PV_H_ */
