/*
 * ocp_zmii.h
 *
 * Defines for the IBM ZMII bridge
 *
 *      Armin Kuster akuster@mvista.com
 *      Dec, 2001
 *
 * Copyright 2001 MontaVista Softare Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR   IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT,  INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  V 1.2 (04/19/02 ) - Armin
 *    added support for emac 2 & 3
 */

#ifndef _OCP_ZMII_H_
#define _OCP_ZMII_H_

#include <linux/config.h>

/* ZMII bridge registers */
struct zmii_regs {
	u32 fer;		/* Function enable reg */
	u32 ssr;		/* Spedd select reg */
	u32 smiirs;		/* SMII status reg */
};

/* ZMII device */
struct ibm_ocp_zmii {
	struct zmii_regs *base;
	int		  mode;  
	int 		  users;/* number of EMACs using this ZMII bridge */
};

/* Fuctional Enable Reg */

#define ZMII_MDI0	0x80000000
#define ZMII_SMII0	0x40000000
#define ZMII_RMII0	0x20000000
#define ZMII_MII0	0x10000000
#define ZMII_MDI1	0x08000000
#define ZMII_SMII1	0x04000000
#define ZMII_RMII1	0x02000000
#define ZMII_MII1	0x01000000
#define ZMII_MDI2	0x00800000
#define ZMII_SMII2	0x00400000
#define ZMII_RMII2	0x00200000
#define ZMII_MII2	0x00100000
#define ZMII_MDI3	0x00080000
#define ZMII_SMII3	0x00040000
#define ZMII_RMII3	0x00020000
#define ZMII_MII3	0x00010000

/* Speed Selection reg */

#define ZMII_SCI0	0x40000000
#define ZMII_FSS0	0x20000000
#define ZMII_SP0	0x10000000
#define ZMII_SCI1	0x04000000
#define ZMII_FSS1	0x02000000
#define ZMII_SP1	0x01000000
#define ZMII_SCI2	0x00400000
#define ZMII_FSS2	0x00200000
#define ZMII_SP2	0x00100000
#define ZMII_SCI3	0x00040000
#define ZMII_FSS3	0x00020000
#define ZMII_SP3	0x00010000

#define ZMII_MII0_100MB	ZMII_SP0
#define ZMII_MII0_10MB	~ZMII_SP0
#define ZMII_MII1_100MB	ZMII_SP1
#define ZMII_MII1_10MB	~ZMII_SP1
#define ZMII_MII2_100MB	ZMII_SP2
#define ZMII_MII2_10MB	~ZMII_SP2
#define ZMII_MII3_100MB	ZMII_SP3
#define ZMII_MII3_10MB	~ZMII_SP3

/* SMII Status reg */

#define ZMII_STS0 0xFF000000	/* EMAC0 smii status mask */
#define ZMII_STS1 0x00FF0000	/* EMAC1 smii status mask */


#define SMII	0
#define RMII	1
#define MII	2
#define MDI	3
#define ZMII_AUTO 4

#endif				/* _OCP_ZMII_H_ */
