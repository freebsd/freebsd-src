#ifndef _INCLUDE_CERF_H_
#define _INCLUDE_CERF_H_

#include <linux/config.h>

#ifdef CONFIG_SA1100_CERF_CPLD


// Map sa1100fb.c to sa1100_frontlight.c - Not pretty, but necessary.
#define CERF_BACKLIGHT_ENABLE   sa1100_fl_enable
#define CERF_BACKLIGHT_DISABLE  sa1100_fl_disable

//
// IO Pins for devices
//

#define CERF_PDA_CPLD             0xf1000000
#define CERF_PDA_CPLD_WRCLRINT    (0x0)
#define CERF_PDA_CPLD_BACKLIGHT   (0x2)
#define CERF_PDA_CPLD_SOUND_FREQ  (0x4)
#define CERF_PDA_CPLD_KEYPAD_A    (0x6)
#define CERF_PDA_CPLD_BATTFAULT   (0x8)
#define CERF_PDA_CPLD_KEYPAD_B    (0xa)
#define CERF_PDA_CPLD_SOUND_ENA   (0xc)
#define CERF_PDA_CPLD_SOUND_RESET (0xe)

#define GPIO_CF_BVD2              GPIO_GPIO (5)
#define GPIO_CF_BVD1              GPIO_GPIO (6)
#define GPIO_CF_RESET             GPIO_GPIO (7)
#define GPIO_CF_IRQ               GPIO_GPIO (8)
#define GPIO_CF_CD                GPIO_GPIO (9)

#define GPIO_PWR_SHUTDOWN         GPIO_GPIO (25)

#define UCB1200_GPIO_CONT_CS      0x0001
#define UCB1200_GPIO_CONT_DOWN    0x0002
#define UCB1200_GPIO_CONT_INC     0x0004
#define UCB1200_GPIO_CONT_ENA     0x0008
#define UCB1200_GPIO_LCD_RESET    0x0010
#define UCB1200_GPIO_IRDA_ENABLE  0x0020
#define UCB1200_GPIO_BT_ENABLE    0x0040
#define UCB1200_GPIO_L3_DATA      0x0080
#define UCB1200_GPIO_L3_CLOCK     0x0100
#define UCB1200_GPIO_L3_MODE      0x0200

//
// IRQ for devices
//

#define IRQ_UCB1200_CONT_CS     IRQ_UCB1200_IO0
#define IRQ_UCB1200_CONT_DOWN   IRQ_UCB1200_IO1
#define IRQ_UCB1200_CONT_INC    IRQ_UCB1200_IO2
#define IRQ_UCB1200_CONT_ENA    IRQ_UCB1200_IO3
#define IRQ_UCB1200_LCD_RESET   IRQ_UCB1200_IO4
#define IRQ_UCB1200_IRDA_ENABLE IRQ_UCB1200_IO5
#define IRQ_UCB1200_BT_ENABLE   IRQ_UCB1200_IO6
#define IRQ_UCB1200_L3_DATA     IRQ_UCB1200_IO7
#define IRQ_UCB1200_L3_CLOCK    IRQ_UCB1200_IO8
#define IRQ_UCB1200_L3_MODE     IRQ_UCB1200_IO9

#define IRQ_GPIO_CF_BVD2        IRQ_GPIO5
#define IRQ_GPIO_CF_BVD1        IRQ_GPIO6
#define IRQ_GPIO_CF_IRQ         IRQ_GPIO8
#define IRQ_GPIO_CF_CD          IRQ_GPIO9

//
// Device parameters
//

#define CERF_PDA_CPLD_SOUND_FREQ_8000  (0x01)
#define CERF_PDA_CPLD_SOUND_FREQ_11025 (0x05)
#define CERF_PDA_CPLD_SOUND_FREQ_16000 (0x02)
#define CERF_PDA_CPLD_SOUND_FREQ_22050 (0x06)
#define CERF_PDA_CPLD_SOUND_FREQ_32000 (0x03)
#define CERF_PDA_CPLD_SOUND_FREQ_44100 (0x07)
#define CERF_PDA_CPLD_SOUND_FREQ_48000 (0x0b)

//
// General Functions
//

#define CERF_PDA_CPLD_Get(x, y)      (*((char*)(CERF_PDA_CPLD + (x))) & (y))
#define CERF_PDA_CPLD_Set(x, y, z)   (*((char*)(CERF_PDA_CPLD + (x))) = (*((char*)(CERF_PDA_CPLD + (x))) & ~(z)) | (y))
#define CERF_PDA_CPLD_UnSet(x, y, z) (*((char*)(CERF_PDA_CPLD + (x))) = (*((char*)(CERF_PDA_CPLD + (x))) & ~(z)) & ~(y))


#else // CONFIG_SA1100_CERF_CPLD


#define GPIO_CF_BVD2            GPIO_GPIO (19)
#define GPIO_CF_BVD1            GPIO_GPIO (20)
#define GPIO_CF_RESET           0
#define GPIO_CF_IRQ             GPIO_GPIO (22)
#define GPIO_CF_CD              GPIO_GPIO (23)

#define GPIO_LCD_RESET          GPIO_GPIO (15)

#define IRQ_GPIO_CF_BVD2        IRQ_GPIO19
#define IRQ_GPIO_CF_BVD1        IRQ_GPIO20
#define IRQ_GPIO_CF_IRQ         IRQ_GPIO22
#define IRQ_GPIO_CF_CD          IRQ_GPIO23


#endif // CONFIG_SA1100_CERF_CPLD


#define GPIO_UCB1200_IRQ        GPIO_GPIO (18)
#define IRQ_GPIO_UCB1200_IRQ    IRQ_GPIO18

#endif // _INCLUDE_CERF_H_
