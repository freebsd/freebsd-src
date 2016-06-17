/*
 * linux/include/asm-arm/arch-sa1100/simpad.h
 *
 * based of assabet.h same as HUW_Webpanel
 *
 * This file contains the hardware specific definitions for SIMpad
 *
 * 2001/05/14 Juergen Messerer <juergen.messerer@siemens.ch>
 */

#ifndef SIMPAD_H
#define SIMPAD_H


#ifndef __ASM_ARCH_HARDWARE_H
#error "include <asm/hardware.h> instead"
#endif

#define GPIO_UART1_RTS	GPIO_GPIO14
#define GPIO_UART1_DTR	GPIO_GPIO7
#define GPIO_UART1_CTS	GPIO_GPIO8
#define GPIO_UART1_DCD	GPIO_GPIO23
#define GPIO_UART1_DSR	GPIO_GPIO6

#define GPIO_UART3_RTS	GPIO_GPIO12
#define GPIO_UART3_DTR	GPIO_GPIO16
#define GPIO_UART3_CTS	GPIO_GPIO13
#define GPIO_UART3_DCD	GPIO_GPIO18
#define GPIO_UART3_DSR	GPIO_GPIO17

#define GPIO_UCB1300_IRQ	GPIO_GPIO (22)	/* UCB GPIO and touchscreen */

#define IRQ_UART1_CTS	IRQ_GPIO15
#define IRQ_UART1_DCD	GPIO_GPIO23
#define IRQ_UART1_DSR	GPIO_GPIO6
#define IRQ_UART3_CTS	GPIO_GPIO13
#define IRQ_UART3_DCD	GPIO_GPIO18
#define IRQ_UART3_DSR	GPIO_GPIO17

#define IRQ_GPIO_UCB1300_IRQ IRQ_GPIO22 

/*---  PCMCIA  ---*/
#define GPIO_CF_CD              GPIO_GPIO24
#define GPIO_CF_IRQ             GPIO_GPIO1          
#define IRQ_GPIO_CF_IRQ         IRQ_GPIO1
#define IRQ_GPIO_CF_CD          IRQ_GPIO24      

// CS3 Latch is write only, a shadow is neccessary 

#define CS3BUSTYPE unsigned volatile long           
#define CS3_BASE        0xf1000000

#define VCC_5V_EN       0x0001 // For 5V PCMCIA 
#define VCC_3V_EN       0x0002 // FOR 3.3V PCMCIA
#define EN1             0x0004 // This is only for EPROM's
#define EN0             0x0008 // Both should be enable for 3.3V or 5V
#define DISPLAY_ON      0x0010
#define PCMCIA_BUFF_DIS 0x0020
#define MQ_RESET        0x0040
#define PCMCIA_RESET    0x0080
#define DECT_POWER_ON   0x0100
#define IRDA_SD         0x0200 // Shutdown for powersave
#define RS232_ON        0x0400
#define SD_MEDIAQ       0x0800 // Shutdown for powersave
#define LED2_ON         0x1000 
#define IRDA_MODE       0x2000 // Fast/Slow IrDA mode
#define ENABLE_5V       0x4000 // Enable 5V circuit
#define RESET_SIMCARD   0x8000

#define RS232_ENABLE    0x0440
#define PCMCIAMASK      0x402f   

#endif // SIMPAD_H








