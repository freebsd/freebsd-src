/*-
 * Copyright (c) 2014 SRI International
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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

#ifndef __CHERI_TCPDUMP_CONTROL_H__
#define __CHERI_TCPDUMP_CONTROL_H__

struct cheri_tcpdump_control {
	int	ctdc_sb_mode;		/* Sandbox mode */
	int	ctdc_sandboxes;		/* Number of sandboxes (mode dep.)*/
	int	ctdc_sb_max_lifetime;	/* Maximum sandbox life (sec) */
	int	ctdc_sb_max_packets;	/* Max packets to process */
	int	ctdc_colorize;		/* Enable colorized output */
	int	ctdc_pause;		/* Pause packet processing */
	int	ctdc_reset;		/* Reset the sandboxes */
};

#define	CTDC_MODE_NONE			0
#define	CTDC_MODE_ONE_SANDBOX		1
#define	CTDC_MODE_SEPARATE_LOCAL	2
#define	CTDC_MODE_HASH_TCP		3

#endif /* __CHERI_TCPDUMP_CONTROL_H__ */
