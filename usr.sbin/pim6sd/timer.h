/*
 * Copyright (C) 1999 LSIIT Laboratory.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 *  Questions concerning this software should be directed to
 *  Mickael Hoerdt (hoerdt@clarinet.u-strasbg.fr) LSIIT Strasbourg.
 *
 */
/*
 * This program has been derived from pim6dd.        
 * The pim6dd program is covered by the license in the accompanying file
 * named "LICENSE.pim6dd".
 */
/*
 * This program has been derived from pimd.        
 * The pimd program is covered by the license in the accompanying file
 * named "LICENSE.pimd".
 *
 * $FreeBSD: src/usr.sbin/pim6sd/timer.h,v 1.1.2.1 2000/07/15 07:36:37 kris Exp $
 */


#ifndef TIMER_H
#define TIMER_H

/* the default granularity if not specified in the config file */

#define DEFAULT_TIMER_INTERVAL 5

/* For timeout. The timers count down */

#define SET_TIMER(timer, value) (timer) = (value)
#define RESET_TIMER(timer) (timer) = 0
#define COPY_TIMER(timer_1, timer_2) (timer_2) = (timer_1)
#define IF_TIMER_SET(timer) if ((timer) > 0)
#define IF_TIMER_NOT_SET(timer) if ((timer) <= 0)
#define FIRE_TIMER(timer)       (timer) = 0


#define IF_TIMER_NOT_SET(timer) if ((timer) <= 0)

#define IF_TIMEOUT(timer)       \
    if (!((timer) -= (MIN(timer, timer_interval))))

#define IF_NOT_TIMEOUT(timer)       \
    if ((timer) -= (MIN(timer, timer_interval)))

#define TIMEOUT(timer)          \
    (!((timer) -= (MIN(timer, timer_interval))))
 
#define NOT_TIMEOUT(timer)      \
    ((timer) -= (MIN(timer, timer_interval)))


extern u_int32 pim_reg_rate_bytes;
extern u_int32 pim_reg_rate_check_interval;
extern u_int32 pim_data_rate_bytes;
extern u_int32 pim_data_rate_check_interval;
extern u_int32 pim_hello_period;
extern u_int32 pim_hello_holdtime;
extern u_int32 timer_interval;
extern u_int32 pim_join_prune_period;
extern u_int32 pim_join_prune_holdtime;
extern u_int32 pim_data_timeout;
extern u_int32 pim_register_suppression_timeout;
extern u_int32 pim_register_probe_time;
extern u_int32 pim_assert_timeout;

extern void init_timers     __P((void));
extern void init_timers     __P((void)); 
extern void age_vifs        __P((void)); 
extern void age_routes      __P((void)); 
extern void age_misc        __P((void));


#endif
