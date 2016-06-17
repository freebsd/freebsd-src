/*
 * linux/include/asm-arm/arch-sa1100/irqs.h
 *
 * Copyright (C) 1996 Russell King
 * Copyright (C) 1998 Deborah Wallach (updates for SA1100/Brutus).
 * Copyright (C) 1999 Nicolas Pitre (full GPIO irq isolation)
 *
 * 2001/11/14	RMK	Cleaned up and standardised a lot of the IRQs.
 */
#include <linux/config.h>

#define	IRQ_GPIO0		0
#define	IRQ_GPIO1		1
#define	IRQ_GPIO2		2
#define	IRQ_GPIO3		3
#define	IRQ_GPIO4		4
#define	IRQ_GPIO5		5
#define	IRQ_GPIO6		6
#define	IRQ_GPIO7		7
#define	IRQ_GPIO8		8
#define	IRQ_GPIO9		9
#define	IRQ_GPIO10		10
#define	IRQ_GPIO11_27		11
#define	IRQ_LCD  		12	/* LCD controller           */
#define	IRQ_Ser0UDC		13	/* Ser. port 0 UDC          */
#define	IRQ_Ser1SDLC		14	/* Ser. port 1 SDLC         */
#define	IRQ_Ser1UART		15	/* Ser. port 1 UART         */
#define	IRQ_Ser2ICP		16	/* Ser. port 2 ICP          */
#define	IRQ_Ser3UART		17	/* Ser. port 3 UART         */
#define	IRQ_Ser4MCP		18	/* Ser. port 4 MCP          */
#define	IRQ_Ser4SSP		19	/* Ser. port 4 SSP          */
#define	IRQ_DMA0 		20	/* DMA controller channel 0 */
#define	IRQ_DMA1 		21	/* DMA controller channel 1 */
#define	IRQ_DMA2 		22	/* DMA controller channel 2 */
#define	IRQ_DMA3 		23	/* DMA controller channel 3 */
#define	IRQ_DMA4 		24	/* DMA controller channel 4 */
#define	IRQ_DMA5 		25	/* DMA controller channel 5 */
#define	IRQ_OST0 		26	/* OS Timer match 0         */
#define	IRQ_OST1 		27	/* OS Timer match 1         */
#define	IRQ_OST2 		28	/* OS Timer match 2         */
#define	IRQ_OST3 		29	/* OS Timer match 3         */
#define	IRQ_RTC1Hz		30	/* RTC 1 Hz clock           */
#define	IRQ_RTCAlrm		31	/* RTC Alarm                */

#define	IRQ_GPIO11		32
#define	IRQ_GPIO12		33
#define	IRQ_GPIO13		34
#define	IRQ_GPIO14		35
#define	IRQ_GPIO15		36
#define	IRQ_GPIO16		37
#define	IRQ_GPIO17		38
#define	IRQ_GPIO18		39
#define	IRQ_GPIO19		40
#define	IRQ_GPIO20		41
#define	IRQ_GPIO21		42
#define	IRQ_GPIO22		43
#define	IRQ_GPIO23		44
#define	IRQ_GPIO24		45
#define	IRQ_GPIO25		46
#define	IRQ_GPIO26		47
#define	IRQ_GPIO27		48

/*
 * To get the GPIO number from an IRQ number
 */
#define GPIO_11_27_IRQ(i)	((i) - 21)

/*
 * The next 16 interrupts are for board specific purposes.  Since
 * the kernel can only run on one machine at a time, we can re-use
 * these.  If you need more, increase IRQ_BOARD_END, but keep it
 * within sensible limits.  IRQs 49 to 64 are available.
 */
#define IRQ_BOARD_START		49
#define IRQ_BOARD_END		65

#define IRQ_SA1111_START	(IRQ_BOARD_END)
#define IRQ_GPAIN0		(IRQ_BOARD_END + 0)
#define IRQ_GPAIN1		(IRQ_BOARD_END + 1)
#define IRQ_GPAIN2		(IRQ_BOARD_END + 2)
#define IRQ_GPAIN3		(IRQ_BOARD_END + 3)
#define IRQ_GPBIN0		(IRQ_BOARD_END + 4)
#define IRQ_GPBIN1		(IRQ_BOARD_END + 5)
#define IRQ_GPBIN2		(IRQ_BOARD_END + 6)
#define IRQ_GPBIN3		(IRQ_BOARD_END + 7)
#define IRQ_GPBIN4		(IRQ_BOARD_END + 8)
#define IRQ_GPBIN5		(IRQ_BOARD_END + 9)
#define IRQ_GPCIN0		(IRQ_BOARD_END + 10)
#define IRQ_GPCIN1		(IRQ_BOARD_END + 11)
#define IRQ_GPCIN2		(IRQ_BOARD_END + 12)
#define IRQ_GPCIN3		(IRQ_BOARD_END + 13)
#define IRQ_GPCIN4		(IRQ_BOARD_END + 14)
#define IRQ_GPCIN5		(IRQ_BOARD_END + 15)
#define IRQ_GPCIN6		(IRQ_BOARD_END + 16)
#define IRQ_GPCIN7		(IRQ_BOARD_END + 17)
#define IRQ_MSTXINT		(IRQ_BOARD_END + 18)
#define IRQ_MSRXINT		(IRQ_BOARD_END + 19)
#define IRQ_MSSTOPERRINT	(IRQ_BOARD_END + 20)
#define IRQ_TPTXINT		(IRQ_BOARD_END + 21)
#define IRQ_TPRXINT		(IRQ_BOARD_END + 22)
#define IRQ_TPSTOPERRINT	(IRQ_BOARD_END + 23)
#define SSPXMTINT		(IRQ_BOARD_END + 24)
#define SSPRCVINT		(IRQ_BOARD_END + 25)
#define SSPROR			(IRQ_BOARD_END + 26)
#define AUDXMTDMADONEA		(IRQ_BOARD_END + 32)
#define AUDRCVDMADONEA		(IRQ_BOARD_END + 33)
#define AUDXMTDMADONEB		(IRQ_BOARD_END + 34)
#define AUDRCVDMADONEB		(IRQ_BOARD_END + 35)
#define AUDTFSR			(IRQ_BOARD_END + 36)
#define AUDRFSR			(IRQ_BOARD_END + 37)
#define AUDTUR			(IRQ_BOARD_END + 38)
#define AUDROR			(IRQ_BOARD_END + 39)
#define AUDDTS			(IRQ_BOARD_END + 40)
#define AUDRDD			(IRQ_BOARD_END + 41)
#define AUDSTO			(IRQ_BOARD_END + 42)
#define USBPWR			(IRQ_BOARD_END + 43)
#define NIRQHCIM		(IRQ_BOARD_END + 44)
#define IRQHCIBUFFACC		(IRQ_BOARD_END + 45)
#define IRQHCIRMTWKP		(IRQ_BOARD_END + 46)
#define NHCIMFCIR		(IRQ_BOARD_END + 47)
#define USB_PORT_RESUME		(IRQ_BOARD_END + 48)
#define S0_READY_NINT		(IRQ_BOARD_END + 49)
#define S1_READY_NINT		(IRQ_BOARD_END + 50)
#define S0_CD_VALID		(IRQ_BOARD_END + 51)
#define S1_CD_VALID		(IRQ_BOARD_END + 52)
#define S0_BVD1_STSCHG		(IRQ_BOARD_END + 53)
#define S1_BVD1_STSCHG		(IRQ_BOARD_END + 54)

/*
 * Figure out the MAX IRQ number.
 *
 * If we have an SA1111, the max IRQ is S1_BVD1_STSCHG+1.
 * If graphicsclient we don't have a SA1111.
 * Otherwise, we have the standard IRQs only.
 */
#ifdef CONFIG_SA1111
#define NR_IRQS			(S1_BVD1_STSCHG + 1)
#elif defined(CONFIG_SA1100_GRAPHICSCLIENT)
#define NR_IRQS			(IRQ_BOARD_END)
#else
#define NR_IRQS			(IRQ_BOARD_START)
#endif

/*
 * Board specific IRQs.  Define them here.
 * Do not surround them with ifdefs.
 */
#define IRQ_NEPONSET_SMC9196	(IRQ_BOARD_START + 0)
#define IRQ_NEPONSET_USAR	(IRQ_BOARD_START + 1)

/* PT Digital Board Interrupts (CONFIG_SA1100_PT_SYSTEM3) */
#define IRQ_SYSTEM3_SMC9196	(IRQ_BOARD_START + 0)

/* ADS Graphics Client IRQs (CONFIG_SA1100_GRAPHICSCLIENT) */
#define IRQ_GRAPHICSCLIENT_START   (IRQ_BOARD_START + 0)
#define IRQ_GRAPHICSCLIENT_CAN     (IRQ_BOARD_START + 4)
#define IRQ_GRAPHICSCLIENT_S0_CD   (IRQ_BOARD_START + 6)
#define IRQ_GRAPHICSCLIENT_EXTIRQ  (IRQ_BOARD_START + 7)
#define IRQ_GRAPHICSCLIENT_UCB1200 (IRQ_BOARD_START + 8)
#define IRQ_GRAPHICSCLIENT_ETH     (IRQ_BOARD_START + 9)
#define IRQ_GRAPHICSCLIENT_USB     (IRQ_BOARD_START + 10)
#define IRQ_GRAPHICSCLIENT_S0_STS  (IRQ_BOARD_START + 11)
#define IRQ_GRAPHICSCLIENT_SWITCH  (IRQ_BOARD_START + 13)
#define IRQ_GRAPHICSCLIENT_AVR     (IRQ_BOARD_START + 14)
#define IRQ_GRAPHICSCLIENT_BATFLT  (IRQ_BOARD_START + 15)
#define IRQ_GRAPHICSCLIENT_END     (IRQ_BOARD_START + 16)

/* ADS Graphics Master IRQs (CONFIG_SA1100_GRAPHICSMASTER) */

#define IRQ_GRAPHICSMASTER_START   (IRQ_BOARD_START)
#define IRQ_GRAPHICSMASTER_SA1111  (IRQ_BOARD_START + 0)
#define IRQ_GRAPHICSMASTER_UART0   (IRQ_BOARD_START + 1)
#define IRQ_GRAPHICSMASTER_UART1   (IRQ_BOARD_START + 2)
#define IRQ_GRAPHICSMASTER_UART2   (IRQ_BOARD_START + 3)
#define IRQ_GRAPHICSMASTER_CAN     (IRQ_BOARD_START + 4)
#define IRQ_GRAPHICSMASTER_UART3   (IRQ_BOARD_START + 5)
#define IRQ_GRAPHICSMASTER_FLASH   (IRQ_BOARD_START + 6)
#define IRQ_GRAPHICSMASTER_EXTIRQ  (IRQ_BOARD_START + 7)
#define IRQ_GRAPHICSMASTER_UCB1200 (IRQ_BOARD_START + 8)
#define IRQ_GRAPHICSMASTER_ETH     (IRQ_BOARD_START + 9)
#define IRQ_GRAPHICSMASTER_SWITCH  (IRQ_BOARD_START + 13)
#define IRQ_GRAPHICSMASTER_AVR     (IRQ_BOARD_START + 14)
#define IRQ_GRAPHICSMASTER_BATFLT  (IRQ_BOARD_START + 15)
#define IRQ_GRAPHICSMASTER_END     (IRQ_BOARD_START + 16)

/* ADS Advanced Graphics Client IRQs (CONFIG_SA1100 ADSAGC) */
#define IRQ_ADSAGC_START           (IRQ_BOARD_START)
#define IRQ_ADSAGC_AVR             (IRQ_BOARD_START + 0)
#define IRQ_ADSAGC_CAN             (IRQ_BOARD_START + 1)
#define IRQ_ADSAGC_ETH             (IRQ_BOARD_START + 2)
#define IRQ_ADSAGC_EXTIRQ          (IRQ_BOARD_START + 3)
#define IRQ_ADSAGC_END             (IRQ_BOARD_START + 4)

