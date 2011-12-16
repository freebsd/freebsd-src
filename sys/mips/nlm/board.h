/*-
 * Copyright 2003-2011 Netlogic Microsystems (Netlogic). All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY Netlogic Microsystems ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETLOGIC OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * NETLOGIC_BSD
 * $FreeBSD$
 */

#ifndef __NLM_BOARD_H__
#define __NLM_BOARD_H__

#define XLP_NAE_NBLOCKS		5
#define XLP_NAE_NPORTS		4
#define	XLP_I2C_MAXDEVICES	8

struct xlp_i2c_devinfo {
	u_int	addr;		/* keep first, for i2c ivars to work */
	int	bus;
	char	*device;
};

struct xlp_port_ivars {
	int	port;
	int	block;
	int	type;
	int	phy_addr;
};

struct xlp_block_ivars {
	int	block;
	int	type;
	u_int	portmask;
	struct xlp_port_ivars	port_ivars[XLP_NAE_NPORTS];
};

struct xlp_nae_ivars {
	int 	node;
	u_int	blockmask;
	struct xlp_block_ivars	block_ivars[XLP_NAE_NBLOCKS];
};

struct xlp_board_info {
	u_int	nodemask;
	struct xlp_node_info {
		struct xlp_i2c_devinfo	i2c_devs[XLP_I2C_MAXDEVICES];
		struct xlp_nae_ivars	nae_ivars;
	} nodes[XLP_MAX_NODES];
};

extern struct xlp_board_info xlp_board_info;
int nlm_board_info_setup(void);

#endif
