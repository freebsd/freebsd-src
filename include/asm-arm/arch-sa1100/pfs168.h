/*
 * linux/include/asm-arm/arch-sa1100/pfs168.h
 *
 * Created 2000/06/05 by Nicolas Pitre <nico@cam.org>
 *
 * This file contains the hardware specific definitions for PFS-168
 *
 */

#ifndef __ASM_ARCH_HARDWARE_H
#error "include <asm/hardware.h> instead"
#endif


/* GPIOs for which the generic definition doesn't say much */
#define GPIO_RADIO_IRQ		GPIO_GPIO (14)	/* Radio interrupt request  */
#define GPIO_L3_I2C_SDA		GPIO_GPIO (15)	/* L3 and SMB control ports */
#define GPIO_PS_MODE_SYNC	GPIO_GPIO (16)	/* Power supply mode/sync   */
#define GPIO_L3_MODE		GPIO_GPIO (17)	/* L3 mode signal with LED  */
#define GPIO_L3_I2C_SCL		GPIO_GPIO (18)	/* L3 and I2C control ports */
#define GPIO_STEREO_64FS_CLK	GPIO_GPIO (19)	/* SSP UDA1341 clock input  */
#define GPIO_CF_IRQ		GPIO_GPIO (21)	/* CF IRQ   */
#define GPIO_MBGNT		GPIO_GPIO (21)	/* 1111 MBGNT */
#define GPIO_CF_CD		GPIO_GPIO (22)	/* CF CD */
#define GPIO_MBREQ		GPIO_GPIO (22)	/* 1111 MBREQ */
#define GPIO_UCB1300_IRQ	GPIO_GPIO (23)	/* UCB GPIO and touchscreen */
#define GPIO_CF_BVD2		GPIO_GPIO (24)	/* CF BVD */
#define GPIO_GFX_IRQ		GPIO_GPIO (24)	/* Graphics IRQ */
#define GPIO_CF_BVD1		GPIO_GPIO (25)	/* CF BVD */
#define GPIO_NEP_IRQ		GPIO_GPIO (25)	/* Neponset IRQ */
#define GPIO_BATT_LOW		GPIO_GPIO (26)	/* Low battery */
#define GPIO_RCLK		GPIO_GPIO (26)	/* CCLK/2  */

#define IRQ_GPIO_CF_IRQ		IRQ_GPIO21
#define IRQ_GPIO_CF_CD		IRQ_GPIO22
#define IRQ_GPIO_MBREQ		IRQ_GPIO22
#define IRQ_GPIO_UCB1300_IRQ	IRQ_GPIO23
#define IRQ_GPIO_CF_BVD2	IRQ_GPIO24
#define IRQ_GPIO_CF_BVD1	IRQ_GPIO25
#define IRQ_GPIO_NEP_IRQ	IRQ_GPIO25


/*
 * PFS-168 definitions:
 */

#define PFS168_SA1111_BASE	(0x40000000)

#ifndef __ASSEMBLY__
#define machine_has_neponset() (0)

#define PFS168_COM5_VBASE		(*((volatile unsigned char *)(0xf0000000UL)))
#define PFS168_COM6_VBASE		(*((volatile unsigned char *)(0xf0001000UL)))
#define PFS168_SYSC1RTS			(*((volatile unsigned char *)(0xf0002000UL)))
#define PFS168_SYSLED			(*((volatile unsigned char *)(0xf0003000UL)))
#define PFS168_SYSDTMF			(*((volatile unsigned char *)(0xf0004000UL)))
#define PFS168_SYSLCDDE			(*((volatile unsigned char *)(0xf0005000UL)))
#define PFS168_SYSC1DSR			(*((volatile unsigned char *)(0xf0006000UL)))
#define PFS168_SYSC3TEN			(*((volatile unsigned char *)(0xf0007000UL)))
#define PFS168_SYSCTLA			(*((volatile unsigned char *)(0xf0008000UL)))
#define PFS168_SYSCTLB			(*((volatile unsigned char *)(0xf0009000UL)))
#define PFS168_ETH_VBASE		(*((volatile unsigned char *)(0xf000a000UL)))
#endif

#define PFS168_SYSLCDDE_STNDE		(1<<0)	/* CSTN display enable/disable (1/0) */
#define PFS168_SYSLCDDE_DESEL		(1<<0)	/* Active/Passive (1/0) display enable mode */

#define PFS168_SYSCTLA_BKLT		(1<<0)	/* LCD backlight invert on/off (1/0) */
#define PFS168_SYSCTLA_RLY		(1<<1)	/* Relay on/off (1/0) */
#define PFS168_SYSCTLA_PXON		(1<<2)	/* Opto relay connect/disconnect 1/0) */
#define PFS168_SYSCTLA_IRDA_FSEL	(1<<3)	/* IRDA Frequency select (0 = SIR, 1 = MIR/ FIR) */

#define PFS168_SYSCTLB_MG1		(1<<0)	/* Motion detector gain select */
#define PFS168_SYSCTLB_MG0		(1<<1)	/* Motion detector gain select */
#define PFS168_SYSCTLB_IRDA_MD1		(1<<2)	/* Range/Power select */
#define PFS168_SYSCTLB_IRDA_MD0		(1<<3)	/* Range/Power select */
#define PFS168_SYSCTLB_IRDA_MD_MASK	(PFS168_SYSCTLB_IRDA_MD1|PFS168_SYSCTLB_IRDA_MD0)
