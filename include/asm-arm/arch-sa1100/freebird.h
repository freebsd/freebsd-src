/*
 *
 */

#ifndef __ASM_ARCH_HARDWARE_H
#error "include <asm/hardware.h> instead"
#endif


/* Board Control Register */

#define BCR_BASE  0xf0000000
#define BCR (*(volatile unsigned int *)(BCR_BASE))

#define BCR_DB1110	(0x00A07410)


#define BCR_FREEBIRD_AUDIO_PWR	(1<<0)	/* Audio Power (1 = on, 0 = off) */
#define BCR_FREEBIRD_LCD_PWR		(1<<1)	/* LCD Power (1 = on) */
#define BCR_FREEBIRD_CODEC_RST	(1<<2)	/* 0 = Holds UCB1300, ADI7171, and UDA1341 in reset */
#define BCR_FREEBIRD_IRDA_FSEL	(1<<3)	/* IRDA Frequency select (0 = SIR, 1 = MIR/ FIR) */
#define BCR_FREEBIRD_IRDA_MD0	(1<<4)	/* Range/Power select */
#define BCR_FREEBIRD_IRDA_MD1	(1<<5)	/* Range/Power select */
#define BCR_FREEBIRD_LCD_DISP	(1<<7)	/* LCD display (1 = on, 0 = off */
#define BCR_FREEBIRD_LCD_BACKLIGHT	(1<<16)	/* LCD backlight ,1=on */
#define BCR_FREEBIRD_LCD_LIGHT_INC	(1<<17)	/* LCD backlight brightness */
#define BCR_FREEBIRD_LCD_LIGHT_DU	(1<<18)	/* LCD backlight brightness */
#define BCR_FREEBIRD_LCD_INC			(1<<19)	/* LCD contrast  */
#define BCR_FREEBIRD_LCD_DU			(1<<20)	/* LCD contrast */
#define BCR_FREEBIRD_QMUTE			(1<<21)	/* Quick Mute */
#define BCR_FREEBIRD_ALARM_LED		(1<<22)	/* ALARM LED control */
#define BCR_FREEBIRD_SPK_OFF	(1<<23)	/* 1 = Speaker amplifier power off */

#ifndef __ASSEMBLY__
extern unsigned long BCR_value;
#define BCR_set( x )	BCR = (BCR_value |= (x))
#define BCR_clear( x )	BCR = (BCR_value &= ~(x))
#endif


/* GPIOs for which the generic definition doesn't say much */
#define GPIO_FREEBIRD_NPOWER_BUTTON		GPIO_GPIO(0)
#define GPIO_FREEBIRD_APP1_BUTTON		GPIO_GPIO(1)
#define GPIO_FREEBIRD_APP2_BUTTON		GPIO_GPIO(2)
#define GPIO_FREEBIRD_APP3_BUTTOM		GPIO_GPIO(3)
#define GPIO_FREEBIRD_UCB1300			GPIO_GPIO(4)

#define GPIO_FREEBIRD_EXPWR				GPIO_GPIO(8)
#define GPIO_FREEBIRD_CHARGING			GPIO_GPIO(9)
#define GPIO_FREEBIRD_RAMD				GPIO_GPIO(14)
#define GPIO_FREEBIRD_L3_DATA			GPIO_GPIO(15)
#define GPIO_FREEBIRD_L3_MODE			GPIO_GPIO(17)
#define GPIO_FREEBIRD_L3_CLOCK			GPIO_GPIO(18)
#define GPIO_FREEBIRD_STEREO_64FS_CLK	GPIO_GPIO(10)

#define GPIO_FREEBIRD_CF_CD				GPIO_GPIO(22)
#define GPIO_FREEBIRD_CF_IRQ			GPIO_GPIO(21)
#define GPIO_FREEBIRD_CF_BVD			GPIO_GPIO(25)

#define IRQ_GPIO_FREEBIRD_NPOWER_BUTTON	IRQ_GPIO0
#define IRQ_GPIO_FREEBIRD_APP1_BUTTON	IRQ_GPIO1
#define IRQ_GPIO_FREEBIRD_APP2_BUTTON	IRQ_GPIO2
#define IRQ_GPIO_FREEBIRD_APP3_BUTTON	IRQ_GPIO3
#define IRQ_GPIO_FREEBIRD_UCB1300_IRQ	IRQ_GPIO4

#define IRQ_GPIO_FREEBIRD_CF_IRQ		IRQ_GPIO21
#define IRQ_GPIO_FREEBIRD_CF_CD			IRQ_GPIO22
#define IRQ_GPIO_FREEBIRD_CF_BVD		IRQ_GPIO25

