/*
 *
 *    Copyright 2000-2001 MontaVista Software Inc.
 *      Original author: Armin Kuster akuster@mvista.com
 *
 *    Module name: ibm405gp.c
 *
 *
 * This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General Public License as published by the
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
 */

#include <linux/config.h>
#include <linux/init.h>
#include <linux/smp.h>
#include <linux/threads.h>
#include <linux/param.h>
#include <linux/string.h>
#include <platforms/ibm405gp.h>
#include <asm/ibm4xx.h>
#include <asm/ocp.h>

static struct ocp_func_emac_data ibm405gp_emac0_def = {
	.zmii_idx	= -1,		/* ZMII device index */
	.zmii_mux	= 0,		/* ZMII input of this EMAC */
	.mal_idx	= 0,		/* MAL device index */
	.mal_rx_chan	= 0,		/* MAL rx channel number */
	.mal_tx1_chan	= 0,		/* MAL tx channel 1 number */
	.mal_tx2_chan	= 1,		/* MAL tx channel 2 number */
	.wol_irq	= BL_MAC_WOL,	/* WOL interrupt number */
	.mdio_idx	= -1,		/* No shared MDIO */
};

static struct ocp_func_mal_data ibm405gp_mal0_def = {
	.num_tx_chans	= 2*EMAC_NUMS,	/* Number of TX channels */
	.num_rx_chans	= EMAC_NUMS,	/* Number of RX channels */
};

struct ocp_def core_ocp[]  __initdata = {
	{ .vendor	= OCP_VENDOR_IBM,
	  .function	= OCP_FUNC_OPB,
	  .index	= 0,
	  .paddr	= OPB_BASE_START,
	  .irq		= OCP_IRQ_NA,
	  .pm		= OCP_CPM_NA,
	},
	{ .vendor	= OCP_VENDOR_IBM,
	  .function	= OCP_FUNC_16550,
	  .index	= 0,
	  .paddr	= UART0_IO_BASE,
	  .irq		= UART0_INT,
	  .pm		= IBM_CPM_UART0
	},
	{ .vendor	= OCP_VENDOR_IBM,
	  .function	= OCP_FUNC_16550,
	  .index	= 1,
	  .paddr	= UART1_IO_BASE,
	  .irq		= UART1_INT,
	  .pm		= IBM_CPM_UART1
	},
	{ .vendor	= OCP_VENDOR_IBM,
	  .function	= OCP_FUNC_IIC,
	  .paddr	= IIC0_BASE,
	  .irq		= IIC0_IRQ,
	  .pm		= IBM_CPM_IIC0
	},
	{ .vendor	= OCP_VENDOR_IBM,
	  .function	= OCP_FUNC_GPIO,
	  .paddr	= GPIO0_BASE,
	  .irq		= OCP_IRQ_NA,
	  .pm		= IBM_CPM_GPIO0
	},
	{ .vendor	= OCP_VENDOR_IBM,
	  .function	= OCP_FUNC_MAL,
	  .paddr	= OCP_PADDR_NA,
	  .irq		= OCP_IRQ_NA,
	  .pm		= OCP_CPM_NA,
	  .additions	= &ibm405gp_mal0_def,
	},
	{ .vendor	= OCP_VENDOR_IBM,
	  .function	= OCP_FUNC_EMAC,
	  .index	= 0,
	  .paddr	= EMAC0_BASE,
	  .irq		= BL_MAC_ETH0,
	  .pm		= IBM_CPM_EMAC0,
	  .additions	= &ibm405gp_emac0_def,
	},
	{ .vendor	= OCP_VENDOR_INVALID
	}
};
