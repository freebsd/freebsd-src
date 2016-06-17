/*
 *
 *    Copyright 2000-2001 MontaVista Software Inc.
 *      Completed implementation.
 *	Current maintainer
 *      Armin Kuster akuster@mvista.com
 *
 *    Module name: ibmstb4.c
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
 *
 *      History: 12/26/2001 - armin
 *		initial release
 *
 *		: 05/05/02 - Armin
 *		  replaced all const strcuts with new core_ocp
 *
 *		: 07/07/02 - Armin
 *		added OCP_IRQ_MUL to define EMAC
 *		and added PM register
 *
 */

#include <linux/init.h>
#include <platforms/ibmstb4.h>
#include <asm/ocp.h>

struct ocp_def core_ocp[] __initdata = {
	{ .vendor	= OCP_VENDOR_IBM,
	  .function	= OCP_FUNC_16550,
	  .index	= 0,
	  .paddr	= UART0_IO_BASE,
	  .irq		= UART0_INT,
	  .pm		= IBM_CPM_UART0,
	},
	{ .vendor	= OCP_VENDOR_IBM,
	  .function	= OCP_FUNC_16550,
	  .index	= 1,
	  .paddr	= UART1_IO_BASE,
	  .irq		= UART1_INT,
	  .pm		= IBM_CPM_UART1,
	},
	{ .vendor	= OCP_VENDOR_IBM,
	  .function	= OCP_FUNC_16550,
	  .index	= 2,
	  .paddr	= UART2_IO_BASE,
	  .irq		= UART2_INT,
	  .pm		= IBM_CPM_UART2,
	},
	{ .vendor	= OCP_VENDOR_IBM,
	  .function	= OCP_FUNC_IIC,
	  .paddr	= IIC0_BASE,
	  .irq		= IIC0_IRQ,
	  .pm		= IBM_CPM_IIC0,
	},
	{ .vendor	= OCP_VENDOR_IBM,
	  .function	= OCP_FUNC_IIC,
	  .paddr	= IIC1_BASE,
	  .irq		= IIC1_IRQ,
	  .pm		= IBM_CPM_IIC1,
	},
	{ .vendor	= OCP_VENDOR_IBM,
	  .function	= OCP_FUNC_GPIO,
	  .paddr	= GPIO0_BASE,
	  .irq		= OCP_IRQ_NA,
	  .pm		= IBM_CPM_GPIO0,
	},
	{ .vendor	= OCP_VENDOR_IBM,
	  .function	= OCP_FUNC_IDE,
	  .paddr	= IDE0_BASE,
	  .irq		= IDE0_IRQ,
	  .pm		= OCP_CPM_NA,
	},
	{ .vendor	= OCP_VENDOR_IBM,
	  .function	= OCP_FUNC_USB,
	  .paddr	= USB0_BASE,
	  .irq		= USB0_IRQ,
	  .pm		= OCP_CPM_NA,
	},
	{ .vendor	= OCP_VENDOR_INVALID,
	}
};
