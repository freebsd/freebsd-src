/*
 * BRIEF MODULE DESCRIPTION
 *	Au1xxx irq map table
 *
 * Copyright 2003 Embedded Edge, LLC
 *		dan@embeddededge.com
 *
 *  This program is free software; you can redistribute	 it and/or modify it
 *  under  the terms of	 the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the	License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED	  ``AS	IS'' AND   ANY	EXPRESS OR IMPLIED
 *  WARRANTIES,	  INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO	EVENT  SHALL   THE AUTHOR  BE	 LIABLE FOR ANY	  DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED	  TO, PROCUREMENT OF  SUBSTITUTE GOODS	OR SERVICES; LOSS OF
 *  USE, DATA,	OR PROFITS; OR	BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN	 CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/kernel_stat.h>
#include <linux/module.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/timex.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <linux/delay.h>

#include <asm/bitops.h>
#include <asm/bootinfo.h>
#include <asm/io.h>
#include <asm/mipsregs.h>
#include <asm/system.h>
#include <asm/au1000.h>

au1xxx_irq_map_t au1xxx_irq_map[] = {
	{ AU1550_UART0_INT, 	INTC_INT_HIGH_LEVEL, 0},
	{ AU1550_PCI_INTA, 		INTC_INT_LOW_LEVEL, 0 },
	{ AU1550_PCI_INTB, 		INTC_INT_LOW_LEVEL, 0 },
	{ AU1550_DDMA_INT, 		INTC_INT_HIGH_LEVEL, 0},
	{ AU1550_CRYPTO_INT, 	INTC_INT_HIGH_LEVEL, 0},
	{ AU1550_PCI_INTC, 		INTC_INT_LOW_LEVEL, 0 },
	{ AU1550_PCI_INTD, 		INTC_INT_LOW_LEVEL, 0 },
	{ AU1550_PCI_RST_INT, 	INTC_INT_LOW_LEVEL, 0 },
	{ AU1550_UART1_INT, 	INTC_INT_HIGH_LEVEL, 0},
	{ AU1550_UART3_INT, 	INTC_INT_HIGH_LEVEL, 0},
	{ AU1550_PSC0_INT, 		INTC_INT_HIGH_LEVEL, 0},
	{ AU1550_PSC1_INT, 		INTC_INT_HIGH_LEVEL, 0},
	{ AU1550_PSC2_INT, 		INTC_INT_HIGH_LEVEL, 0},
	{ AU1550_PSC3_INT, 		INTC_INT_HIGH_LEVEL, 0},
	{ AU1550_TOY_INT, 		INTC_INT_RISE_EDGE, 0 },
	{ AU1550_TOY_MATCH0_INT,INTC_INT_RISE_EDGE, 0 },
	{ AU1550_TOY_MATCH1_INT,INTC_INT_RISE_EDGE, 0 },
	/* Careful if you change match 2 request!
	 * The interrupt handler is called directly
	 * from the low level dispatch code.
	 */
	{ AU1550_TOY_MATCH2_INT,INTC_INT_RISE_EDGE, 0 },
	{ AU1550_RTC_INT, 		INTC_INT_RISE_EDGE, 0 },
	{ AU1550_RTC_MATCH0_INT,INTC_INT_RISE_EDGE, 0 },
	{ AU1550_RTC_MATCH1_INT,INTC_INT_RISE_EDGE, 0 },
	{ AU1550_RTC_MATCH2_INT,INTC_INT_RISE_EDGE, 0 },
	{ AU1550_RTC_MATCH2_INT,INTC_INT_RISE_EDGE, 0 },
	{ AU1550_NAND_INT, 		INTC_INT_RISE_EDGE, 0 },
	{ AU1550_USB_DEV_REQ_INT, INTC_INT_RISE_EDGE, 0 },
	{ AU1550_USB_DEV_SUS_INT, INTC_INT_RISE_EDGE, 0 },
	{ AU1550_USB_HOST_INT,  INTC_INT_LOW_LEVEL, 0 },
	{ AU1550_MAC0_DMA_INT,  INTC_INT_HIGH_LEVEL, 0},
	{ AU1550_MAC1_DMA_INT,  INTC_INT_HIGH_LEVEL, 0},


	/*
	 *  Need to define platform dependant GPIO ints here
	 */
	#warning PbAu1550 needs GPIO Interrupts defined

};

int au1xxx_nr_irqs = sizeof(au1xxx_irq_map)/sizeof(au1xxx_irq_map_t);
