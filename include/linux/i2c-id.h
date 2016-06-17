/* ------------------------------------------------------------------------- */
/* 									     */
/* i2c.h - definitions for the i2c-bus interface			     */
/* 									     */
/* ------------------------------------------------------------------------- */
/*   Copyright (C) 1995-1999 Simon G. Vogl

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.		     */
/* ------------------------------------------------------------------------- */

/* $Id: i2c-id.h,v 1.35 2001/08/12 17:22:20 mds Exp $ */

#ifndef I2C_ID_H
#define I2C_ID_H
/*
 * This file is part of the i2c-bus package and contains the identifier
 * values for drivers, adapters and other folk populating these serial
 * worlds. 
 *
 * These will change often (i.e. additions) , therefore this has been 
 * separated from the functional interface definitions of the i2c api.
 *
 */

/*
 * ---- Driver types -----------------------------------------------------
 *       device id name + number        function description, i2c address(es)
 *
 *  Range 1000-1999 range is defined in sensors/sensors.h 
 *  Range 0x100 - 0x1ff is for V4L2 Common Components 
 *  Range 0xf000 - 0xffff is reserved for local experimentation, and should
 *        never be used in official drivers 
 */

#define I2C_DRIVERID_MSP3400	 1
#define I2C_DRIVERID_TUNER	 2
#define I2C_DRIVERID_VIDEOTEX	 3	/* please rename		*/
#define I2C_DRIVERID_TDA8425	 4	/* stereo sound processor	*/
#define I2C_DRIVERID_TEA6420	 5	/* audio matrix switch		*/
#define I2C_DRIVERID_TEA6415C	 6	/* video matrix switch		*/
#define I2C_DRIVERID_TDA9840	 7	/* stereo sound processor	*/
#define I2C_DRIVERID_SAA7111A	 8	/* video input processor	*/
#define I2C_DRIVERID_SAA5281	 9	/* videotext decoder		*/
#define I2C_DRIVERID_SAA7112	10	/* video decoder, image scaler	*/
#define I2C_DRIVERID_SAA7120	11	/* video encoder		*/
#define I2C_DRIVERID_SAA7121	12	/* video encoder		*/
#define I2C_DRIVERID_SAA7185B	13	/* video encoder		*/
#define I2C_DRIVERID_CH7003	14	/* digital pc to tv encoder 	*/
#define I2C_DRIVERID_PCF8574A	15	/* i2c expander - 8 bit in/out	*/
#define I2C_DRIVERID_PCF8582C	16	/* eeprom			*/
#define I2C_DRIVERID_AT24Cxx	17	/* eeprom 1/2/4/8/16 K 		*/
#define I2C_DRIVERID_TEA6300	18	/* audio mixer			*/
#define I2C_DRIVERID_BT829	19	/* pc to tv encoder		*/
#define I2C_DRIVERID_TDA9850	20	/* audio mixer			*/
#define I2C_DRIVERID_TDA9855	21	/* audio mixer			*/
#define I2C_DRIVERID_SAA7110	22	/* video decoder		*/
#define I2C_DRIVERID_MGATVO	23	/* Matrox TVOut			*/
#define I2C_DRIVERID_SAA5249	24	/* SAA5249 and compatibles	*/
#define I2C_DRIVERID_PCF8583	25	/* real time clock		*/
#define I2C_DRIVERID_SAB3036	26	/* SAB3036 tuner		*/
#define I2C_DRIVERID_TDA7432	27	/* Stereo sound processor	*/
#define I2C_DRIVERID_TVMIXER    28      /* Mixer driver for tv cards    */
#define I2C_DRIVERID_TVAUDIO    29      /* Generic TV sound driver      */
#define I2C_DRIVERID_DPL3518    30      /* Dolby decoder chip           */
#define I2C_DRIVERID_TDA9873    31      /* TV sound decoder chip        */
#define I2C_DRIVERID_TDA9875    32      /* TV sound decoder chip        */
#define I2C_DRIVERID_PIC16C54_PV9 33    /* Audio mux/ir receiver        */

#define I2C_DRIVERID_SBATT      34     /* Smart Battery Device		*/
#define I2C_DRIVERID_SBS        35     /* SB System Manager		*/
#define I2C_DRIVERID_VES1893	36     /* VLSI DVB-S decoder		*/
#define I2C_DRIVERID_VES1820	37     /* VLSI DVB-C decoder		*/
#define I2C_DRIVERID_SAA7113	38     /* video decoder			*/
#define I2C_DRIVERID_TDA8444	39     /* octuple 6-bit DAC             */
#define I2C_DRIVERID_BT819	40     /* video decoder			*/
#define I2C_DRIVERID_BT856	41     /* video encoder			*/
#define I2C_DRIVERID_VPX32XX	42     /* video decoder+vbi/vtxt	*/
#define I2C_DRIVERID_DRP3510	43     /* ADR decoder (Astra Radio)	*/
#define I2C_DRIVERID_SP5055	44     /* Satellite tuner		*/
#define I2C_DRIVERID_STV0030	45     /* Multipurpose switch		*/
#define I2C_DRIVERID_ADV7175	48     /* ADV 7175/7176 video encoder	*/
#define I2C_DRIVERID_MAX1617	56     /* temp sensor			*/
#define I2C_DRIVERID_SAA7191	57     /* video decoder                 */
#define I2C_DRIVERID_INDYCAM	58     /* SGI IndyCam			*/

#define I2C_DRIVERID_EXP0	0xF0	/* experimental use id's	*/
#define I2C_DRIVERID_EXP1	0xF1
#define I2C_DRIVERID_EXP2	0xF2
#define I2C_DRIVERID_EXP3	0xF3

#define I2C_DRIVERID_I2CDEV	900
#define I2C_DRIVERID_I2CPROC	901

/* IDs --   Use DRIVERIDs 1000-1999 for sensors. 
   These were originally in sensors.h in the lm_sensors package */
#define I2C_DRIVERID_LM78 1002
#define I2C_DRIVERID_LM75 1003
#define I2C_DRIVERID_GL518 1004
#define I2C_DRIVERID_EEPROM 1005
#define I2C_DRIVERID_W83781D 1006
#define I2C_DRIVERID_LM80 1007
#define I2C_DRIVERID_ADM1021 1008
#define I2C_DRIVERID_ADM9240 1009
#define I2C_DRIVERID_LTC1710 1010
#define I2C_DRIVERID_SIS5595 1011
#define I2C_DRIVERID_ICSPLL 1012
#define I2C_DRIVERID_BT869 1013
#define I2C_DRIVERID_MAXILIFE 1014
#define I2C_DRIVERID_MATORB 1015
#define I2C_DRIVERID_GL520 1016
#define I2C_DRIVERID_THMC50 1017
#define I2C_DRIVERID_DDCMON 1018
#define I2C_DRIVERID_VIA686A 1019
#define I2C_DRIVERID_ADM1025 1020
#define I2C_DRIVERID_LM87 1021
#define I2C_DRIVERID_PCF8574 1022
#define I2C_DRIVERID_MTP008 1023
#define I2C_DRIVERID_DS1621 1024
#define I2C_DRIVERID_ADM1024 1025
#define I2C_DRIVERID_IT87 1026
#define I2C_DRIVERID_CH700X 1027 /* single driver for CH7003-7009 digital pc to tv encoders */

/*
 * ---- Adapter types ----------------------------------------------------
 *
 * First, we distinguish between several algorithms to access the hardware
 * interface types, as a PCF 8584 needs other care than a bit adapter.
 */

#define I2C_ALGO_NONE	0x000000
#define I2C_ALGO_BIT	0x010000	/* bit style adapters		*/
#define I2C_ALGO_PCF	0x020000	/* PCF 8584 style adapters	*/
#define I2C_ALGO_ATI	0x030000	/* ATI video card		*/
#define I2C_ALGO_SMBUS	0x040000
#define I2C_ALGO_ISA 	0x050000	/* lm_sensors ISA pseudo-adapter */
#define I2C_ALGO_SAA7146 0x060000	/* SAA 7146 video decoder bus	*/
#define I2C_ALGO_ACB 	0x070000	/* ACCESS.bus algorithm         */

#define I2C_ALGO_EC     0x100000        /* ACPI embedded controller     */

#define I2C_ALGO_MPC8XX 0x110000	/* MPC8xx PowerPC I2C algorithm */

#define I2C_ALGO_SIBYTE 0x120000	/* Broadcom SiByte SOCs		*/

#define I2C_ALGO_SGI	0x130000	/* SGI algorithm		*/

#define I2C_ALGO_EXP	0x800000	/* experimental			*/

#define I2C_ALGO_MASK	0xff0000	/* Mask for algorithms		*/
#define I2C_ALGO_SHIFT	0x10	/* right shift to get index values 	*/

#define I2C_HW_ADAPS	0x10000		/* # adapter types		*/
#define I2C_HW_MASK	0xffff		


/* hw specific modules that are defined per algorithm layer
 */

/* --- Bit algorithm adapters 						*/
#define I2C_HW_B_LP	0x00	/* Parallel port Philips style adapter	*/
#define I2C_HW_B_LPC	0x01	/* Parallel port, over control reg.	*/
#define I2C_HW_B_SER	0x02	/* Serial line interface		*/
#define I2C_HW_B_ELV	0x03	/* ELV Card				*/
#define I2C_HW_B_VELLE	0x04	/* Vellemann K8000			*/
#define I2C_HW_B_BT848	0x05	/* BT848 video boards			*/
#define I2C_HW_B_WNV	0x06	/* Winnov Videums			*/
#define I2C_HW_B_VIA	0x07	/* Via vt82c586b			*/
#define I2C_HW_B_HYDRA	0x08	/* Apple Hydra Mac I/O			*/
#define I2C_HW_B_G400	0x09	/* Matrox G400				*/
#define I2C_HW_B_I810	0x0a	/* Intel I810 				*/
#define I2C_HW_B_VOO	0x0b	/* 3dfx Voodoo 3 / Banshee      	*/
#define I2C_HW_B_PPORT  0x0c	/* Primitive parallel port adapter	*/
#define I2C_HW_B_RIVA	0x10	/* Riva based graphics cards		*/
#define I2C_HW_B_IOC	0x11	/* IOC bit-wiggling			*/
#define I2C_HW_B_TSUNA  0x12	/* DEC Tsunami chipset			*/

/* --- PCF 8584 based algorithms					*/
#define I2C_HW_P_LP	0x00	/* Parallel port interface		*/
#define I2C_HW_P_ISA	0x01	/* generic ISA Bus inteface card	*/
#define I2C_HW_P_ELEK	0x02	/* Elektor ISA Bus inteface card	*/

/* --- ACPI Embedded controller algorithms                              */
#define I2C_HW_ACPI_EC          0x00

/* --- MPC8xx PowerPC adapters						*/
#define I2C_HW_MPC8XX_EPON 0x00	/* Eponymous MPC8xx I2C adapter 	*/

/* --- Broadcom SiByte adapters						*/
#define I2C_HW_SIBYTE	0x00

/* --- SGI adapters							*/
#define I2C_HW_SGI_VINO	0x00
#define I2C_HW_SGI_MACE	0x01

/* --- SMBus only adapters						*/
#define I2C_HW_SMBUS_PIIX4	0x00
#define I2C_HW_SMBUS_ALI15X3	0x01
#define I2C_HW_SMBUS_VIA2	0x02
#define I2C_HW_SMBUS_VOODOO3	0x03
#define I2C_HW_SMBUS_I801	0x04
#define I2C_HW_SMBUS_AMD756	0x05
#define I2C_HW_SMBUS_SIS5595	0x06
#define I2C_HW_SMBUS_ALI1535	0x07
#define I2C_HW_SMBUS_W9968CF	0x0d

/* --- ISA pseudo-adapter						*/
#define I2C_HW_ISA 0x00

#endif /* I2C_ID_H */
