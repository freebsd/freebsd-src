/*
 * 	Author: MontaVista Software, Inc.
 *          Armin Kuster
 *
 *    Module name: redwood6.h
 *
 *    Description:
 *      Macros, definitions, and data structures specific to the IBM PowerPC
 *      STBx25xx "Redwood6" evaluation board.
 *
 *    Copyright 2002 MontaVista Software Inc.
 * 	Author: MontaVista Software, Inc.
 *          Armin Kuster
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 * THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR   IMPLIED
 * WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT,  INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 * USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * You should have received a copy of the  GNU General Public License along
 * with this program; if not, write  to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifdef __KERNEL__
#ifndef __ASM_REDWOOD5_H__
#define __ASM_REDWOOD5_H__

/* Redwood6 has an STBx25xx core */
#include <platforms/ibmstbx25.h>

#ifndef __ASSEMBLY__
typedef struct board_info {
	unsigned char	bi_s_version[4];	/* Version of this structure */
	unsigned char	bi_r_version[30];	/* Version of the IBM ROM */
	unsigned int	bi_memsize;		/* DRAM installed, in bytes */
	unsigned int	bi_dummy;		/* field shouldn't exist */
	unsigned char	bi_enetaddr[6];		/* Ethernet MAC address */
	unsigned int	bi_intfreq;		/* Processor speed, in Hz */
	unsigned int	bi_busfreq;		/* Bus speed, in Hz */
	unsigned int	bi_tbfreq;		/* Software timebase freq */
	unsigned int	bi_opb_busfreq;		/* OPB Bus speed, in Hz */
	int		bi_iic_fast[1];		/* Use fast i2c mode */
} bd_t;
#endif				/* !__ASSEMBLY__ */

#define SMC91111_BASE_ADDR	0xf2030300
#define SMC91111_IRQ		27
#define IDE_XLINUX_MUX_BASE        0xf2040000
#define IDE_DMA_ADDR	0xfce00000

#ifdef MAX_HWIFS
#undef MAX_HWIFS
#endif
#define MAX_HWIFS		1

#define _IO_BASE	0
#define _ISA_MEM_BASE	0
#define PCI_DRAM_OFFSET	0

#define BASE_BAUD		(378000000 / 18 / 16)

#define PPC4xx_MACHINE_NAME	"IBM Redwood6"

#endif				/* __ASM_REDWOOD5_H__ */
#endif				/* __KERNEL__ */
