/*
 * include/asm-mips/vr41xx/vr41xx.h
 *
 * Include file for NEC VR4100 series.
 *
 * Copyright (C) 1999 Michael Klar
 * Copyright (C) 2001, 2002 Paul Mundt
 * Copyright (C) 2002 MontaVista Software, Inc.
 * Copyright (C) 2002 TimeSys Corp.
 * Copyright (C) 2003 Yoichi Yuasa <yuasa@hh.iij4u.or.jp>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */
#ifndef __NEC_VR41XX_H
#define __NEC_VR41XX_H

#include <linux/interrupt.h>

/*
 * CPU Revision
 */
/* VR4122 0x00000c70-0x00000c72 */
#define PRID_VR4122_REV1_0	0x00000c70
#define PRID_VR4122_REV2_0	0x00000c70
#define PRID_VR4122_REV2_1	0x00000c70
#define PRID_VR4122_REV3_0	0x00000c71
#define PRID_VR4122_REV3_1	0x00000c72

/* VR4181A 0x00000c73-0x00000c7f */
#define PRID_VR4181A_REV1_0	0x00000c73
#define PRID_VR4181A_REV1_1	0x00000c74

/* VR4131 0x00000c80-0x00000c83 */
#define PRID_VR4131_REV1_2	0x00000c80
#define PRID_VR4131_REV2_0	0x00000c81
#define PRID_VR4131_REV2_1	0x00000c82
#define PRID_VR4131_REV2_2	0x00000c83

/* VR4133 0x00000c84- */
#define PRID_VR4133		0x00000c84

/*
 * Bus Control Uint
 */
extern void vr41xx_bcu_init(void);
extern unsigned long vr41xx_get_vtclock_frequency(void);
extern unsigned long vr41xx_get_tclock_frequency(void);

/*
 * Clock Mask Unit
 */
extern void vr41xx_cmu_init(void);
extern void vr41xx_clock_supply(unsigned int clock);
extern void vr41xx_clock_mask(unsigned int clock);

enum {
	PIU_CLOCK,
	SIU_CLOCK,
	AIU_CLOCK,
	KIU_CLOCK,
	FIR_CLOCK,
	DSIU_CLOCK,
	CSI_CLOCK,
	PCIU_CLOCK,
	HSP_CLOCK,
	PCI_CLOCK,
	CEU_CLOCK,
	ETHER0_CLOCK,
	ETHER1_CLOCK
};

/*
 * Interrupt Control Unit
 */
/* CPU core Interrupt Numbers */
#define MIPS_CPU_IRQ_BASE	0
#define MIPS_CPU_IRQ(x)		(MIPS_CPU_IRQ_BASE + (x))
#define MIPS_SOFTINT0_IRQ	MIPS_CPU_IRQ(0)
#define MIPS_SOFTINT1_IRQ	MIPS_CPU_IRQ(1)
#define INT0_CASCADE_IRQ	MIPS_CPU_IRQ(2)
#define INT1_CASCADE_IRQ	MIPS_CPU_IRQ(3)
#define INT2_CASCADE_IRQ	MIPS_CPU_IRQ(4)
#define INT3_CASCADE_IRQ	MIPS_CPU_IRQ(5)
#define INT4_CASCADE_IRQ	MIPS_CPU_IRQ(6)
#define MIPS_COUNTER_IRQ	MIPS_CPU_IRQ(7)

/* SYINT1 Interrupt Numbers */
#define SYSINT1_IRQ_BASE	8
#define SYSINT1_IRQ(x)		(SYSINT1_IRQ_BASE + (x))
#define BATTRY_IRQ		SYSINT1_IRQ(0)
#define POWER_IRQ		SYSINT1_IRQ(1)
#define RTCLONG1_IRQ		SYSINT1_IRQ(2)
#define ELAPSEDTIME_IRQ		SYSINT1_IRQ(3)
/* RFU */
#define PIU_IRQ			SYSINT1_IRQ(5)
#define AIU_IRQ			SYSINT1_IRQ(6)
#define KIU_IRQ			SYSINT1_IRQ(7)
#define GIUINT_CASCADE_IRQ	SYSINT1_IRQ(8)
#define SIU_IRQ			SYSINT1_IRQ(9)
#define BUSERR_IRQ		SYSINT1_IRQ(10)
#define SOFTINT_IRQ		SYSINT1_IRQ(11)
#define CLKRUN_IRQ		SYSINT1_IRQ(12)
#define DOZEPIU_IRQ		SYSINT1_IRQ(13)
#define SYSINT1_IRQ_LAST	DOZEPIU_IRQ

/* SYSINT2 Interrupt Numbers */
#define SYSINT2_IRQ_BASE	24
#define SYSINT2_IRQ(x)		(SYSINT2_IRQ_BASE + (x))
#define RTCLONG2_IRQ		SYSINT2_IRQ(0)
#define LED_IRQ			SYSINT2_IRQ(1)
#define HSP_IRQ			SYSINT2_IRQ(2)
#define TCLOCK_IRQ		SYSINT2_IRQ(3)
#define FIR_IRQ			SYSINT2_IRQ(4)
#define CEU_IRQ			SYSINT2_IRQ(4)	/* same number as FIR_IRQ */
#define DSIU_IRQ		SYSINT2_IRQ(5)
#define PCI_IRQ			SYSINT2_IRQ(6)
#define SCU_IRQ			SYSINT2_IRQ(7)
#define CSI_IRQ			SYSINT2_IRQ(8)
#define BCU_IRQ			SYSINT2_IRQ(9)
#define ETHERNET_IRQ		SYSINT2_IRQ(10)
#define SYSINT2_IRQ_LAST	ETHERNET_IRQ

/* GIU Interrupt Numbers */
#define GIU_IRQ_BASE		40
#define GIU_IRQ(x)		(GIU_IRQ_BASE + (x))	/* IRQ 40-71 */
#define GIU_IRQ_LAST		GIU_IRQ(31)
#define GIU_IRQ_TO_PIN(x)	((x) - GIU_IRQ_BASE)	/* Pin 0-31 */

extern void (*board_irq_init)(void);
extern int vr41xx_set_intassign(unsigned int irq, unsigned char intassign);
extern int vr41xx_cascade_irq(unsigned int irq, int (*get_irq_number)(int irq));

/*
 * RTC
 */
extern void vr41xx_set_rtclong1_cycle(uint32_t cycles);
extern uint32_t vr41xx_read_rtclong1_counter(void);

extern void vr41xx_set_rtclong2_cycle(uint32_t cycles);
extern uint32_t vr41xx_read_rtclong2_counter(void);

extern void vr41xx_set_tclock_cycle(uint32_t cycles);
extern uint32_t vr41xx_read_tclock_counter(void);

/*
 * General-Purpose I/O Unit
 */
enum {
	TRIGGER_LEVEL,
	TRIGGER_EDGE,
	TRIGGER_EDGE_FALLING,
	TRIGGER_EDGE_RISING
};

enum {
	SIGNAL_THROUGH,
	SIGNAL_HOLD
};

extern void vr41xx_set_irq_trigger(int pin, int trigger, int hold);

enum {
	LEVEL_LOW,
	LEVEL_HIGH
};

extern void vr41xx_set_irq_level(int pin, int level);

enum {
	PIO_INPUT,
	PIO_OUTPUT
};

enum {
	DATA_LOW,
	DATA_HIGH
};

/*
 * Serial Interface Unit
 */
extern void vr41xx_siu_init(int interface, int module);
extern void vr41xx_siu_ifselect(int interface, int module);
extern int vr41xx_serial_ports;

/* SIU interfaces */
enum {
	SIU_RS232C,
	SIU_IRDA
};

/* IrDA interfaces */
enum {
	IRDA_SHARP = 1,
	IRDA_TEMIC,
	IRDA_HP
};

/*
 * Debug Serial Interface Unit
 */
extern void vr41xx_dsiu_init(void);

/*
 * PCI Control Unit
 */
struct vr41xx_pci_address_space {
	u32 internal_base;
	u32 address_mask;
	u32 pci_base;
};

struct vr41xx_pci_address_map {
	struct vr41xx_pci_address_space *mem1;
	struct vr41xx_pci_address_space *mem2;
	struct vr41xx_pci_address_space *io;
};

extern void vr41xx_pciu_init(struct vr41xx_pci_address_map *map);

/*
 * MISC
 */
extern void vr41xx_time_init(void);
extern void vr41xx_timer_setup(struct irqaction *irq);

extern void vr41xx_restart(char *command);
extern void vr41xx_halt(void);
extern void vr41xx_power_off(void);

#if defined(CONFIG_IDE) || defined(CONFIG_IDE_MODULE)
extern struct ide_ops vr41xx_ide_ops;
#endif

#endif /* __NEC_VR41XX_H */
