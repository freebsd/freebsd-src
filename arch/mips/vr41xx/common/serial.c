/*
 * FILE NAME
 *	arch/mips/vr41xx/common/serial.c
 *
 * BRIEF MODULE DESCRIPTION
 *	Serial Interface Unit routines for NEC VR4100 series.
 *
 * Author: Yoichi Yuasa
 *         yyuasa@mvista.com or source@mvista.com
 *
 * Copyright 2002 MontaVista Software Inc.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version.
 *
 *  THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 *  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 *  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 *  OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 *  TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 *  USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 */
/*
 * Changes:
 *  MontaVista Software Inc. <yyuasa@mvista.com> or <source@mvista.com>
 *  - New creation, NEC VR4122 and VR4131 are supported.
 *  - Added support for NEC VR4111 and VR4121.
 *
 *  Yoichi Yuasa <yuasa@hh.iij4u.or.jp>
 *  - Added support for NEC VR4133.
 */
#include <linux/init.h>
#include <linux/types.h>
#include <linux/serial.h>

#include <asm/addrspace.h>
#include <asm/cpu.h>
#include <asm/io.h>
#include <asm/vr41xx/vr41xx.h>

/* VR4111 and VR4121 SIU Registers */
#define SIURB_TYPE1		KSEG1ADDR(0x0c000000)
#define SIUIRSEL_TYPE1		KSEG1ADDR(0x0c000008)

/* VR4122, VR4131 and VR4133 SIU Registers */
#define SIURB_TYPE2		KSEG1ADDR(0x0f000800)
#define SIUIRSEL_TYPE2		KSEG1ADDR(0x0f000808)

 #define USE_RS232C		0x00
 #define USE_IRDA		0x01
 #define SIU_USES_IRDA		0x00
 #define FIR_USES_IRDA		0x02
 #define IRDA_MODULE_SHARP	0x00
 #define IRDA_MODULE_TEMIC	0x04
 #define IRDA_MODULE_HP		0x08
 #define TMICTX			0x10
 #define TMICMODE		0x20

#define SIU_BASE_BAUD		1152000

/* VR4122 and VR4131 DSIU Registers */
#define DSIURB			KSEG1ADDR(0x0f000820)

#define MDSIUINTREG		KSEG1ADDR(0x0f000096)
 #define INTDSIU		0x0800

#define DSIU_BASE_BAUD		1152000

int vr41xx_serial_ports = 0;

void vr41xx_siu_ifselect(int interface, int module)
{
	u16 val = USE_RS232C;	/* Select RS-232C */

	/* Select IrDA */
	if (interface == SIU_IRDA) {
		switch (module) {
		case IRDA_SHARP:
			val = IRDA_MODULE_SHARP;
			break;
		case IRDA_TEMIC:
			val = IRDA_MODULE_TEMIC;
			break;
		case IRDA_HP:
			val = IRDA_MODULE_HP;
			break;
		}
		val |= USE_IRDA | SIU_USES_IRDA;
	}

	switch (current_cpu_data.cputype) {
	case CPU_VR4111:
	case CPU_VR4121:
		writew(val, SIUIRSEL_TYPE1);
		break;
	case CPU_VR4122:
	case CPU_VR4131:
	case CPU_VR4133:
		writew(val, SIUIRSEL_TYPE2);
		break;
	default:
		printk(KERN_INFO "Unexpected CPU of NEC VR4100 series\n");
		break;
	}
}

void __init vr41xx_siu_init(int interface, int module)
{
	struct serial_struct s;

	vr41xx_siu_ifselect(interface, module);

	memset(&s, 0, sizeof(s));

	s.line = vr41xx_serial_ports;
	s.baud_base = SIU_BASE_BAUD;
	s.irq = SIU_IRQ;
	s.flags = ASYNC_BOOT_AUTOCONF | ASYNC_SKIP_TEST;
	switch (current_cpu_data.cputype) {
	case CPU_VR4111:
	case CPU_VR4121:
		s.iomem_base = (unsigned char *)SIURB_TYPE1;
		break;
	case CPU_VR4122:
	case CPU_VR4131:
	case CPU_VR4133:
		s.iomem_base = (unsigned char *)SIURB_TYPE2;
		break;
	default:
		panic("Unexpected CPU of NEC VR4100 series");
		break;
	}
	s.iomem_reg_shift = 0;
	s.io_type = SERIAL_IO_MEM;
	if (early_serial_setup(&s) != 0)
		printk(KERN_ERR "SIU setup failed!\n");

	vr41xx_clock_supply(SIU_CLOCK);

	vr41xx_serial_ports++;
}

void __init vr41xx_dsiu_init(void)
{
	struct serial_struct s;

	if (current_cpu_data.cputype != CPU_VR4122 &&
	    current_cpu_data.cputype != CPU_VR4131 &&
	    current_cpu_data.cputype != CPU_VR4133)
		return;

	memset(&s, 0, sizeof(s));

	s.line = vr41xx_serial_ports;
	s.baud_base = DSIU_BASE_BAUD;
	s.irq = DSIU_IRQ;
	s.flags = ASYNC_BOOT_AUTOCONF | ASYNC_SKIP_TEST;
	s.iomem_base = (unsigned char *)DSIURB;
	s.iomem_reg_shift = 0;
	s.io_type = SERIAL_IO_MEM;
	if (early_serial_setup(&s) != 0)
		printk(KERN_ERR "DSIU setup failed!\n");

	vr41xx_clock_supply(DSIU_CLOCK);

	writew(INTDSIU, MDSIUINTREG);

	vr41xx_serial_ports++;
}
