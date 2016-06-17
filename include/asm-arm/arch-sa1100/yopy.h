#ifndef __ASM_ARCH_YOPY_H__
#define __ASM_ARCH_YOPY_H__

/******************************************************************************
 * Memory mappings
 ******************************************************************************/

/* Flash memories */
#define YOPY_FLASH0_BASE_P	(0x00000000)	/* CS0 */
#define YOPY_FLASH0_BASE_V	(0xe8000000)
#define YOPY_FLASH0_BASE	YOPY_FLASH0_BASE_V
#define YOPY_FLASH0_SIZE	(0x04000000)	/* map 64MB */

#define YOPY_FLASH1_BASE_P	(0x08000000)	/* CS1 */
#define YOPY_FLASH1_BASE_V	(YOPY_FLASH0_BASE_V + YOPY_FLASH0_SIZE)
#define YOPY_FLASH1_BASE	YOPY_FLASH1_BASE_V
#define YOPY_FLASH1_SIZE	(0x04000000)	/* map 64MB */

/* LCD Controller */
#define YOPY_LCD_IO_BASE_P	(0x48000000)	/* CS5 */
#define YOPY_LCD_IO_BASE_V	(0xf0000000)

#define YOPY_LCD_IO_BASE	YOPY_LCD_IO_BASE_V
#define YOPY_LCD_IO_RANGE	(0x00208000)

/* Extended GPIO */
#define YOPY_EGPIO_BASE_P	(0x10000000)	/* CS2 */
#define YOPY_EGPIO_BASE_V	(0xf1000000)

#define YOPY_EGPIO_BASE		YOPY_EGPIO_BASE_V
#define YOPY_EGPIO_RANGE	4

#define YOPY_EGPIO		(*((volatile Word *)YOPY_EGPIO_BASE))


/******************************************************************************
 * GPIO assignements
 ******************************************************************************/

#define GPIO_UCB1200_IRQ	GPIO_GPIO0
#define GPIO_UCB1200_RESET	GPIO_GPIO22

#define GPIO_CF_IREQ		GPIO_GPIO2
#define GPIO_CF_CD		GPIO_GPIO3
#define GPIO_CF_BVD1		GPIO_GPIO4
#define GPIO_CF_BVD2		GPIO_GPIO5
#define GPIO_CF_CSEL		GPIO_GPIO6
#define GPIO_CF_READY		GPIO_CF_IREQ
#define GPIO_CF_STSCHG		GPIO_CF_BVD1
#define GPIO_CF_SPKR		GPIO_CF_BVD2

#define GPIO_MASK(io)		(1 << (io))

#define GPIO_YOPY_PLL_ML	PPC_LDD7
#define GPIO_YOPY_PLL_MC	PPC_L_LCLK
#define GPIO_YOPY_PLL_MD	PPC_L_FCLK

#define GPIO_YOPY_L3_MODE	PPC_LDD4
#define GPIO_YOPY_L3_CLOCK	PPC_LDD5
#define GPIO_YOPY_L3_DATA	PPC_LDD6

#define GPIO_CF_RESET		0
#define GPIO_CLKDIV_CLR1	1
#define GPIO_CLKDIV_CLR2	2
#define GPIO_SPEAKER_MUTE	5
#define GPIO_CF_POWER		8
#define GPIO_AUDIO_OPAMP_POWER	11
#define GPIO_AUDIO_CODEC_POWER	12
#define GPIO_AUDIO_POWER	13

#define GPIO_IRDA_POWER		PPC_L_PCLK
#define GPIO_IRDA_FIR		PPC_LDD0

#ifndef __ASSEMBLY__
extern int yopy_gpio_test(unsigned int gpio);
extern void yopy_gpio_set(unsigned int gpio, int level);
#endif


/******************************************************************************
 * IRQ assignements
 ******************************************************************************/

/* for our old drivers */
#define IRQ_SP0_UDC	13
#define IRQ_SP1_SDLC	14
#define IRQ_SP1_UART	15
#define IRQ_SP2_ICP	16
#define IRQ_SP2_UART	16
#define IRQ_SP3_UART	17
#define IRQ_SP4_MCP	18
#define IRQ_SP4_SSP	19
#define IRQ_RTC_HZ	30
#define IRQ_RTC_ALARM	31

/* GPIO interrupts */
#define IRQ_GPIO_UCB1200_IRQ	IRQ_GPIO0

#define IRQ_CF_IREQ		IRQ_GPIO2
#define IRQ_CF_CD		IRQ_GPIO3
#define IRQ_CF_BVD1		IRQ_GPIO4
#define IRQ_CF_BVD2		IRQ_GPIO5

#define IRQ_UART_CTS		IRQ_GPIO7
#define IRQ_UART_DCD		IRQ_GPIO8
#define IRQ_UART_DSR		IRQ_GPIO9

#define IRQ_FLASH_STATUS	IRQ_GPIO23

#define IRQ_BUTTON_POWER	IRQ_GPIO1
#define IRQ_BUTTON_UP		IRQ_GPIO14
#define IRQ_BUTTON_DOWN		IRQ_GPIO15
#define IRQ_BUTTON_LEFT		IRQ_GPIO16
#define IRQ_BUTTON_RIGHT	IRQ_GPIO17
#define IRQ_BUTTON_SHOT0	IRQ_GPIO18
#define IRQ_BUTTON_SHOT1	IRQ_GPIO20
#define IRQ_BUTTON_PIMS		IRQ_UCB1200_IO1
#define IRQ_BUTTON_MP3		IRQ_UCB1200_IO2
#define IRQ_BUTTON_RECORD	IRQ_UCB1200_IO3
#define IRQ_BUTTON_PREV		IRQ_UCB1200_IO4
#define IRQ_BUTTON_SELECT	IRQ_UCB1200_IO5
#define IRQ_BUTTON_NEXT		IRQ_UCB1200_IO6
#define IRQ_BUTTON_CANCEL	IRQ_UCB1200_IO7
#define IRQ_BUTTON_REMOTE	IRQ_UCB1200_IO8


#endif
