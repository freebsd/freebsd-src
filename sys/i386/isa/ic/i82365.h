#ifndef __83265_H__
#define __83265_H__

/***********************************************************************
 * 82365.h -- information necessary for direct manipulation of PCMCIA
 * cards and controllers
 *
 * Support is included for Intel 82365SL PCIC controllers and clones
 * thereof.
 *
 * originally by Barry Jaspan; hacked over by Keith Moore
 *
 * $FreeBSD$
 ***********************************************************************/

/*
 * PCIC Registers
 *     Each register is given a name, and most of the bits are named too.
 *     I should really name them all.
 *
 *     Finally, since the banks can be addressed with a regular syntax,
 *     some macros are provided for that purpose.
 */

#define PCIC_BASE 0x03e0	/* base adddress of pcic register set */

/* First, all the registers */
#define PCIC_ID_REV	0x00	/* Identification and Revision */
#define PCIC_STATUS	0x01	/* Interface Status */
#define PCIC_POWER	0x02	/* Power and RESETDRV control */
#define PCIC_INT_GEN	0x03	/* Interrupt and General Control */
#define PCIC_STAT_CHG	0x04	/* Card Status Change */
#define PCIC_STAT_INT	0x05	/* Card Status Change Interrupt Config */
#define PCIC_ADDRWINE	0x06	/* Address Window Enable */
#define PCIC_IOCTL	0x07	/* I/O Control */
#define PCIC_IO0_STL	0x08	/* I/O Address 0 Start Low Byte */
#define PCIC_IO0_STH	0x09	/* I/O Address 0 Start High Byte */
#define PCIC_IO0_SPL	0x0a	/* I/O Address 0 Stop Low Byte */
#define PCIC_IO0_SPH	0x0b	/* I/O Address 0 Stop High Byte */
#define PCIC_IO1_STL	0x0c	/* I/O Address 1 Start Low Byte */
#define PCIC_IO1_STH	0x0d	/* I/O Address 1 Start High Byte */
#define PCIC_IO1_SPL	0x0e	/* I/O Address 1 Stop Low Byte */
#define PCIC_IO1_SPH	0x0f	/* I/O Address 1 Stop High Byte */
#define PCIC_SM0_STL	0x10	/* System Memory Address 0 Mapping Start Low Byte */
#define PCIC_SM0_STH	0x11	/* System Memory Address 0 Mapping Start High Byte */
#define PCIC_SM0_SPL	0x12	/* System Memory Address 0 Mapping Stop Low Byte */
#define PCIC_SM0_SPH	0x13	/* System Memory Address 0 Mapping Stop High Byte */
#define PCIC_CM0_L	0x14	/* Card Memory Offset Address 0 Low Byte */
#define PCIC_CM0_H	0x15	/* Card Memory Offset Address 0 High Byte */
#define PCIC_CDGC	0x16	/* Card Detect and General Control */
#define PCIC_RES17	0x17	/* Reserved */
#define PCIC_SM1_STL	0x18	/* System Memory Address 1 Mapping Start Low Byte */
#define PCIC_SM1_STH	0x19	/* System Memory Address 1 Mapping Start High Byte */
#define PCIC_SM1_SPL	0x1a	/* System Memory Address 1 Mapping Stop Low Byte */
#define PCIC_SM1_SPH	0x1b	/* System Memory Address 1 Mapping Stop High Byte */
#define PCIC_CM1_L	0x1c	/* Card Memory Offset Address 1 Low Byte */
#define PCIC_CM1_H	0x1d	/* Card Memory Offset Address 1 High Byte */
#define PCIC_GLO_CTRL	0x1e	/* Global Control Register */
#define PCIC_RES1F	0x1f	/* Reserved */
#define PCIC_SM2_STL	0x20	/* System Memory Address 2 Mapping Start Low Byte */
#define PCIC_SM2_STH	0x21	/* System Memory Address 2 Mapping Start High Byte */
#define PCIC_SM2_SPL	0x22	/* System Memory Address 2 Mapping Stop Low Byte */
#define PCIC_SM2_SPH	0x23	/* System Memory Address 2 Mapping Stop High Byte */
#define PCIC_CM2_L	0x24	/* Card Memory Offset Address 2 Low Byte */
#define PCIC_CM2_H	0x25	/* Card Memory Offset Address 2 High Byte */
#define PCIC_RES26	0x26	/* Reserved */
#define PCIC_RES27	0x27	/* Reserved */
#define PCIC_SM3_STL	0x28	/* System Memory Address 3 Mapping Start Low Byte */
#define PCIC_SM3_STH	0x29	/* System Memory Address 3 Mapping Start High Byte */
#define PCIC_SM3_SPL	0x2a	/* System Memory Address 3 Mapping Stop Low Byte */
#define PCIC_SM3_SPH	0x2b	/* System Memory Address 3 Mapping Stop High Byte */
#define PCIC_CM3_L	0x2c	/* Card Memory Offset Address 3 Low Byte */
#define PCIC_CM3_H	0x2d	/* Card Memory Offset Address 3 High Byte */
#define PCIC_RES2E	0x2e	/* Reserved */
#define PCIC_RES2F	0x2f	/* Reserved */
#define PCIC_SM4_STL	0x30	/* System Memory Address 4 Mapping Start Low Byte */
#define PCIC_SM4_STH	0x31	/* System Memory Address 4 Mapping Start High Byte */
#define PCIC_SM4_SPL	0x32	/* System Memory Address 4 Mapping Stop Low Byte */
#define PCIC_SM4_SPH	0x33	/* System Memory Address 4 Mapping Stop High Byte */
#define PCIC_CM4_L	0x34	/* Card Memory Offset Address 4 Low Byte */
#define PCIC_CM4_H	0x35	/* Card Memory Offset Address 4 High Byte */
#define PCIC_RES36	0x36	/* Reserved */
#define PCIC_RES37	0x37	/* Reserved */
#define PCIC_RES38	0x38	/* Reserved */
#define PCIC_RES39	0x39	/* Reserved */
#define PCIC_RES3A	0x3a	/* Reserved */
#define PCIC_RES3B	0x3b	/* Reserved */
#define PCIC_RES3C	0x3c	/* Reserved */
#define PCIC_RES3D	0x3d	/* Reserved */
#define PCIC_RES3E	0x3e	/* Reserved */
#define PCIC_RES3F	0x3f	/* Reserved */

/* Now register bits, ordered by reg # */

/* For Identification and Revision (PCIC_ID_REV) */
#define PCIC_INTEL0	0x82	/* Intel 82365SL Rev. 0; Both Memory and I/O */
#define PCIC_INTEL1	0x83	/* Intel 82365SL Rev. 1; Both Memory and I/O */
#define PCIC_IBM1	0x88	/* IBM PCIC clone; Both Memory and I/O */
#define PCIC_IBM2	0x89	/* IBM PCIC clone; Both Memory and I/O */

/* For Interface Status register (PCIC_STATUS) */
#define PCIC_VPPV	0x80	/* Vpp_valid */
#define PCIC_POW	0x40	/* PC Card power active */
#define PCIC_READY	0x20	/* Ready/~Busy */
#define PCIC_MWP	0x10	/* Memory Write Protect */
#define PCIC_CD		0x0C	/* Both card detect bits */
#define PCIC_BVD	0x03	/* Both Battery Voltage Detect bits */

/* For the Power and RESETDRV register (PCIC_POWER) */
#define PCIC_OUTENA	0x80	/* Output Enable */
#define PCIC_DISRST	0x40	/* Disable RESETDRV */
#define PCIC_APSENA	0x20	/* Auto Pwer Switch Enable */
#define PCIC_PCPWRE	0x10	/* PC Card Power Enable */

/* For the Interrupt and General Control register (PCIC_INT_GEN) */
#define PCIC_CARDTYPE	0x20	/* Card Type 0 = memory, 1 = I/O */
#define		PCIC_IOCARD	0x20
#define		PCIC_MEMCARD	0x00
#define PCIC_CARDRESET	0x40	/* Card reset 0 = Reset, 1 = Normal */

/* For the Card Status Change register (PCIC_STAT_CHG) */
#define PCIC_CDTCH	0x08	/* Card Detect Change */
#define PCIC_RDYCH	0x04	/* Ready Change */
#define PCIC_BATWRN	0x02	/* Battery Warning */
#define PCIC_BATDED	0x01	/* Battery Dead */

/* For the Address Window Enable Register (PCIC_ADDRWINE) */
#define PCIC_SM0_EN	0x01	/* Memory Window 0 Enable */
#define PCIC_SM1_EN	0x02	/* Memory Window 1 Enable */
#define PCIC_SM2_EN	0x04	/* Memory Window 2 Enable */
#define PCIC_SM3_EN	0x08	/* Memory Window 3 Enable */
#define PCIC_SM4_EN	0x10	/* Memory Window 4 Enable */
#define PCIC_MEMCS16	0x20	/* ~MEMCS16 Decode A23-A12 */
#define PCIC_IO0_EN	0x40	/* I/O Window 0 Enable */
#define PCIC_IO1_EN	0x80	/* I/O Window 1 Enable */

/* For the I/O Control Register (PCIC_IOCTL) */
#define PCIC_IO0_16BIT	0x01	/* I/O to this segment is 16 bit */
#define PCIC_IO0_CS16	0x02	/* I/O cs16 source is the card */
#define PCIC_IO0_0WS	0x04	/* zero wait states added on 8 bit cycles */
#define PCIC_IO0_WS	0x08	/* Wait states added for 16 bit cycles */
#define PCIC_IO1_16BIT	0x10	/* I/O to this segment is 16 bit */
#define PCIC_IO1_CS16	0x20	/* I/O cs16 source is the card */
#define PCIC_IO1_0WS	0x04	/* zero wait states added on 8 bit cycles */
#define PCIC_IO1_WS	0x80	/* Wait states added for 16 bit cycles */

/* For the various I/O and Memory windows */
#define PCIC_ADDR_LOW	0
#define PCIC_ADDR_HIGH	1
#define PCIC_START	0x00	/* Start of mapping region */
#define PCIC_END	0x02	/* End of mapping region */
#define PCIC_MOFF	0x04	/* Card Memory Mapping region offset */
#define PCIC_IO0	0x08	/* I/O Address 0 */
#define PCIC_IO1	0x0c	/* I/O Address 1 */
#define PCIC_SM0	0x10	/* System Memory Address 0 Mapping */
#define PCIC_SM1	0x18	/* System Memory Address 1 Mapping */
#define PCIC_SM2	0x20	/* System Memory Address 2 Mapping */
#define PCIC_SM3	0x28	/* System Memory Address 3 Mapping */
#define PCIC_SM4	0x30	/* System Memory Address 4 Mapping */

/* For System Memory Window start registers
   (PCIC_SMx|PCIC_START|PCIC_ADDR_HIGH) */
#define PCIC_ZEROWS	0x40	/* Zero wait states */
#define PCIC_DATA16	0x80	/* Data width is 16 bits */

/* For System Memory Window stop registers
   (PCIC_SMx|PCIC_END|PCIC_ADDR_HIGH) */
#define PCIC_MW0	0x40	/* Wait state bit 0 */
#define PCIC_MW1	0x80	/* Wait state bit 1 */

/* For System Memory Window offset registers
   (PCIC_SMx|PCIC_MOFF|PCIC_ADDR_HIGH) */
#define PCIC_REG	0x40	/* Attribute/Common select (why called Reg?) */
#define PCIC_WP		0x80	/* Write-protect this window */

/* For Card Detect and General Control register (PCIC_CDGC) */
#define PCIC_16_DL_INH	0x01	/* 16-bit memory delay inhibit */
#define PCIC_CNFG_RST_EN 0x02	/* configuration reset enable */
#define PCIC_GPI_EN	0x04	/* GPI Enable */
#define PCIC_GPI_TRANS	0x08	/* GPI Transition Control */
#define PCIC_CDRES_EN	0x10	/* card detect resume enable */
#define PCIC_SW_CD_INT	0x20	/* s/w card detect interrupt */

/* For Global Control register (PCIC_GLO_CTRL) */
#define PCIC_PWR_DOWN	0x01	/* power down */
#define PCIC_LVL_MODE	0x02	/* level mode interrupt enable */
#define PCIC_WB_CSCINT	0x04	/* explicit write-back csc intr */
#define PCIC_IRQ14_PULSE 0x08	/* irq 14 pulse mode enable */

/* DON'T ADD ANYTHING AFTER THIS #endif */
#endif /* __83265_H__ */
