/*
 *    Copyright (c) 2000 Mike Corrigan <mikejc@us.ibm.com>
 *    Copyright (c) 1999-2000 Grant Erickson <grant@lcse.umn.edu>
 *
 *    Module name: as400_setup.h
 *
 *    Description:
 *      Architecture- / platform-specific boot-time initialization code for
 *      the IBM AS/400 LPAR. Adapted from original code by Grant Erickson and
 *      code by Gary Thomas, Cort Dougan <cort@cs.nmt.edu>, and Dan Malek
 *      <dan@netx4.com>.
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */

#ifndef	__ISERIES_SETUP_H__
#define	__ISERIES_SETUP_H__

extern void		 iSeries_init_early(void);
extern void		 iSeries_init(unsigned long r3,
			            unsigned long ird_start,
				    unsigned long ird_end,
				    unsigned long cline_start,
				    unsigned long cline_end);
extern void		 iSeries_setup_arch(void);
extern void		 iSeries_setup_residual(struct seq_file *m);
extern void		 iSeries_get_cpuinfo(struct seq_file *m);
extern void		 iSeries_init_IRQ(void);
extern int		 iSeries_get_irq(struct pt_regs *regs);
extern void		 iSeries_restart(char *cmd);
extern void		 iSeries_power_off(void);
extern void		 iSeries_halt(void);
extern void		 iSeries_time_init(void);
extern void      iSeries_get_boot_time(struct rtc_time *tm);
extern int		 iSeries_set_rtc_time(unsigned long now);
extern unsigned long	 iSeries_get_rtc_time(void);
extern void		 iSeries_calibrate_decr(void);
extern void 	 iSeries_progress( char *, unsigned short );

#endif /* __ISERIES_SETUP_H__ */
