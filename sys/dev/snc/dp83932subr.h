/*	$FreeBSD$	*/
/*	$NecBSD: dp83932subr.h,v 1.5 1999/02/02 00:47:25 kmatsuda Exp $	*/
/*	$NetBSD$	*/
  
/*
 * Copyright (c) 1997, 1998, 1999
 *	Kouichi Matsuda.  All rights reserved.
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
 *      This product includes software developed by Kouichi Matsuda for
 *      NetBSD/pc98.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 */
/*
 * Routines of NEC PC-9801-83, 84, 103, 104, PC-9801N-25 and PC-9801N-J02, J02R 
 * Ethernet interface for NetBSD/pc98, ported by Kouichi Matsuda.
 *
 * These cards use National Semiconductor DP83934AVQB as Ethernet Controller
 * and National Semiconductor NS46C46 as (64 * 16 bits) Microwire Serial EEPROM.
 */

int sncsetup __P((struct snc_softc *, u_int8_t *));

u_int8_t snc_nec16_validate_irq __P((int));
int snc_nec16_register_irq __P((struct snc_softc *, int));
int snc_nec16_validate_mem __P((int));
int snc_nec16_register_mem __P((struct snc_softc *, int));

u_int16_t snc_nec16_nic_get __P((struct snc_softc *, u_int8_t));
void snc_nec16_nic_put __P((struct snc_softc *, u_int8_t, u_int16_t));


void snc_nec16_writetodesc
	__P((struct snc_softc *, u_int32_t, u_int32_t, u_int16_t));
u_int16_t snc_nec16_readfromdesc
	__P((struct snc_softc *, u_int32_t, u_int32_t));

void snc_nec16_copyfrombuf __P((struct snc_softc *, void *, u_int32_t, size_t));
void snc_nec16_copytobuf __P((struct snc_softc *, void *, u_int32_t, size_t));
void snc_nec16_zerobuf __P((struct snc_softc *, u_int32_t, size_t));

int snc_nec16_detectsubr
	__P((bus_space_tag_t, bus_space_handle_t, bus_space_tag_t,
		bus_space_handle_t, int, int, u_int8_t));
int snc_nec16_check_memory
	__P((bus_space_tag_t, bus_space_handle_t, bus_space_tag_t,
		bus_space_handle_t));

int snc_nec16_get_enaddr
	__P((bus_space_tag_t, bus_space_handle_t, u_int8_t *));
u_int8_t *snc_nec16_detect_type __P((u_int8_t *));
void snc_nec16_read_eeprom
	__P((bus_space_tag_t, bus_space_handle_t, u_int8_t *));

#ifdef	SNCDEBUG
void snc_nec16_dump_reg __P((bus_space_tag_t, bus_space_handle_t));
#endif	/* SNDEBUG */
