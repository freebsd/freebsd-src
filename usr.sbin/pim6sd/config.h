/*
 * Copyright (C) 1999 LSIIT Laboratory.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 *  Questions concerning this software should be directed to
 *  Mickael Hoerdt (hoerdt@clarinet.u-strasbg.fr) LSIIT Strasbourg.
 *
 */
/*
 * This program has been derived from pim6dd.        
 * The pim6dd program is covered by the license in the accompanying file
 * named "LICENSE.pim6dd".
 */
/*
 * This program has been derived from pimd.        
 * The pimd program is covered by the license in the accompanying file
 * named "LICENSE.pimd".
 *
 * $FreeBSD: src/usr.sbin/pim6sd/config.h,v 1.1.2.1 2000/07/15 07:36:35 kris Exp $
 */

#ifndef CONFIG_H
#define CONFIG_H

#define UNKNOWN        -1
#define EMPTY           1
#define PHYINT          2
#define CANDIDATE_RP    3
#define GROUP_PREFIX    4
#define BOOTSTRAP_RP    5
#define REG_THRESHOLD   6 
#define DATA_THRESHOLD  7
#define DEFAULT_SOURCE_METRIC     8
#define DEFAULT_SOURCE_PREFERENCE 9
#define HELLO_PERIOD			  10
#define GRANULARITY				  11
#define JOIN_PRUNE_PERIOD		  12
#define DATA_TIMEOUT		13
#define REGISTER_SUPPRESSION_TIMEOUT 14 
#define PROBE_TIME			15
#define ASSERT_TIMEOUT			16

#define EQUAL(s1, s2)            (strcmp((s1), (s2)) == 0)

void config_vifs_from_kernel __P((void));
void config_vifs_from_file __P((void));

#endif
