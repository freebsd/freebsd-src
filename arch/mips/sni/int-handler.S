/*
 * SNI RM200 PCI specific interrupt handler code.
 *
 * Copyright (C) 1994, 95, 96, 97, 98, 1999, 2000, 01 by Ralf Baechle
 */
#include <asm/asm.h>
#include <asm/mipsregs.h>
#include <asm/regdef.h>
#include <asm/sni.h>
#include <asm/stackframe.h>

/*
 * The PCI ASIC has the nasty property that it may delay writes if it is busy.
 * As a consequence from writes that have not graduated when we exit from the
 * interrupt handler we might catch a spurious interrupt.  To avoid this we
 * force the PCI ASIC to graduate all writes by executing a read from the
 * PCI bus.
 */
		.set	noreorder
		.set	noat
		.align	5
		NESTED(sni_rm200_pci_handle_int, PT_SIZE, sp)
		SAVE_ALL
		CLI
		.set	at

		/* Blinken light ...  */
		lb	t0, led_cache
		addiu	t0, 1
		sb	t0, led_cache
		sb	t0, PCIMT_CSLED			# write only register
		.data
led_cache:	.byte	0
		.text

		mfc0	t0, CP0_STATUS
		mfc0	t1, CP0_CAUSE
		and	t0, t1

		 andi	t1, t0, 0x4a00			# hardware interrupt 1
		bnez	t1, _hwint134
		 andi	t1, t0, 0x1000			# hardware interrupt 2
		bnez	t1, _hwint2
		 andi	t1, t0, 0x8000			# hardware interrupt 5
		bnez	t1, _hwint5
		 andi	t1, t0, 0x0400			# hardware interrupt 0
		bnez	t1, _hwint0
		 nop

		j	ret_from_irq			# spurious interrupt
		 nop

 ##############################################################################

/* hwint0 should deal with MP agent, ASIC PCI, EISA NMI and debug
   button interrupts.  */
_hwint0:	jal	pciasic_hwint0
		 move	a1, sp

/*
 * hwint 1 deals with EISA and SCSI interrupts,
 * hwint 3 should deal with the PCI A - D interrupts,
 * hwint 4 is used for only the onboard PCnet 32.
 */
_hwint134:	jal	pciasic_hwint134


/* This interrupt was used for the com1 console on the first prototypes.  */
_hwint2:	jal	pciasic_hwint2

/* hwint5 is the r4k count / compare interrupt  */
_hwint5:	jal	pciasic_hwint5

		END(sni_rm200_pci_handle_int)
