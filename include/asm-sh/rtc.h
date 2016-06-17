#ifndef _ASM_RTC_H
#define _ASM_RTC_H

#include <asm/machvec.h>

#define rtc_gettimeofday sh_mv.mv_rtc_gettimeofday
#define rtc_settimeofday sh_mv.mv_rtc_settimeofday

extern void sh_rtc_gettimeofday(struct timeval *tv);
extern int sh_rtc_settimeofday(const struct timeval *tv);

/* RCR1 Bits */
#define RCR1_CF		0x80	/* Carry Flag             */
#define RCR1_CIE	0x10	/* Carry Interrupt Enable */
#define RCR1_AIE	0x08	/* Alarm Interrupt Enable */
#define RCR1_AF		0x01	/* Alarm Flag             */

/* RCR2 Bits */
#define RCR2_PEF	0x80	/* PEriodic interrupt Flag */
#define RCR2_PESMASK	0x70	/* Periodic interrupt Set  */
#define RCR2_RTCEN	0x08	/* ENable RTC              */
#define RCR2_ADJ	0x04	/* ADJustment (30-second)  */
#define RCR2_RESET	0x02	/* Reset bit               */
#define RCR2_START	0x01	/* Start bit               */

#if defined(__sh3__)
/* SH-3 RTC */
#define R64CNT  	0xfffffec0
#define RSECCNT 	0xfffffec2
#define RMINCNT 	0xfffffec4
#define RHRCNT  	0xfffffec6
#define RWKCNT  	0xfffffec8
#define RDAYCNT 	0xfffffeca
#define RMONCNT 	0xfffffecc
#define RYRCNT  	0xfffffece
#define RSECAR  	0xfffffed0
#define RMINAR  	0xfffffed2
#define RHRAR   	0xfffffed4
#define RWKAR   	0xfffffed6
#define RDAYAR  	0xfffffed8
#define RMONAR  	0xfffffeda
#define RCR1    	0xfffffedc
#define RCR2    	0xfffffede

#define RTC_BIT_INVERTED	0	/* No bug on SH7708, SH7709A */
#elif defined(__SH4__)
/* SH-4 RTC */
#define R64CNT  	0xffc80000
#define RSECCNT 	0xffc80004
#define RMINCNT 	0xffc80008
#define RHRCNT  	0xffc8000c
#define RWKCNT  	0xffc80010
#define RDAYCNT 	0xffc80014
#define RMONCNT 	0xffc80018
#define RYRCNT  	0xffc8001c  /* 16bit */
#define RSECAR  	0xffc80020
#define RMINAR  	0xffc80024
#define RHRAR   	0xffc80028
#define RWKAR   	0xffc8002c
#define RDAYAR  	0xffc80030
#define RMONAR  	0xffc80034
#define RCR1    	0xffc80038
#define RCR2    	0xffc8003c

#define RTC_BIT_INVERTED	0x40	/* bug on SH7750, SH7750S */
#endif

#endif /* _ASM_RTC_H */
