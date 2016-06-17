#ifndef _saa7191_h_
#define _saa7191_h_

/* Phillips SAA7191 DMSD I2C bus address */
#define SAA7191_ADDR		0x8a

/* Register subaddresses. */
#define SAA7191_REG_IDEL	0x00
#define SAA7191_REG_HSYB	0x01
#define SAA7191_REG_HSYS	0x02
#define SAA7191_REG_HCLB	0x03
#define SAA7191_REG_HCLS	0x04
#define SAA7191_REG_HPHI	0x05
#define SAA7191_REG_LUMA	0x06
#define SAA7191_REG_HUEC	0x07
#define SAA7191_REG_CKTQ	0x08
#define SAA7191_REG_CKTS	0x09
#define SAA7191_REG_PLSE	0x0a
#define SAA7191_REG_SESE	0x0b
#define SAA7191_REG_GAIN	0x0c
#define SAA7191_REG_STDC	0x0d
#define SAA7191_REG_IOCK	0x0e
#define SAA7191_REG_CTL3	0x0f
#define SAA7191_REG_CTL4	0x10
#define SAA7191_REG_CHCV	0x11
#define SAA7191_REG_HS6B	0x14
#define SAA7191_REG_HS6S	0x15
#define SAA7191_REG_HC6B	0x16
#define SAA7191_REG_HC6S	0x17
#define SAA7191_REG_HP6I	0x18
#define SAA7191_REG_STATUS	0xff	/* not really a subaddress */

/* Status Register definitions */
#define SAA7191_STATUS_CODE	0x01	/* color detected flag */
#define SAA7191_STATUS_FIDT	0x20	/* format type NTSC/PAL */
#define SAA7191_STATUS_HLCK	0x40	/* PLL unlocked/locked */
#define SAA7191_STATUS_STTC	0x80	/* tv/vtr time constant */

/* Luminance Control Register definitions */
#define SAA7191_LUMA_BYPS	0x80

/* I/O and Clock Control Register definitions */
#define SAA7191_IOCK_HPLL	0x80
#define SAA7191_IOCK_CHRS	0x04

#endif
