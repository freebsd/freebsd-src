/*-
 * Copyright (c) 1998
 *	HD Associates, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by HD Associates, Inc
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY HD ASSOCIATES AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL HD ASSOCIATES OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/unistd.h>

static int facility[CTL_POSIX4_N_CTLS];

#define P4_SYSCTL(num, name)  \
 SYSCTL_INT(_posix4, num, name, CTLFLAG_RD, facility + num - 1, 0, "");

P4_SYSCTL(CTL_POSIX4_ASYNCHRONOUS_IO, asynchronous_io);
P4_SYSCTL(CTL_POSIX4_MAPPED_FILES, mapped_files);
P4_SYSCTL(CTL_POSIX4_MEMLOCK, memlock);
P4_SYSCTL(CTL_POSIX4_MEMLOCK_RANGE, memlock_range);
P4_SYSCTL(CTL_POSIX4_MEMORY_PROTECTION, memory_protection);
P4_SYSCTL(CTL_POSIX4_MESSAGE_PASSING, message_passing);
P4_SYSCTL(CTL_POSIX4_PRIORITIZED_IO, prioritized_io);
P4_SYSCTL(CTL_POSIX4_PRIORITY_SCHEDULING, priority_scheduling);
P4_SYSCTL(CTL_POSIX4_REALTIME_SIGNALS, realtime_signals);
P4_SYSCTL(CTL_POSIX4_SEMAPHORES, semaphores);
P4_SYSCTL(CTL_POSIX4_FSYNC, fsync);
P4_SYSCTL(CTL_POSIX4_SHARED_MEMORY_OBJECTS, shared_memory_objects);
P4_SYSCTL(CTL_POSIX4_SYNCHRONIZED_IO, synchronized_io);
P4_SYSCTL(CTL_POSIX4_TIMERS, timers);
P4_SYSCTL(CTL_POSIX4_AIO_LISTIO_MAX, aio_listio_max);
P4_SYSCTL(CTL_POSIX4_AIO_MAX, aio_max);
P4_SYSCTL(CTL_POSIX4_AIO_PRIO_DELTA_MAX, aio_prio_delta_max);
P4_SYSCTL(CTL_POSIX4_DELAYTIMER_MAX, delaytimer_max);
P4_SYSCTL(CTL_POSIX4_MQ_OPEN_MAX, mq_open_max);
P4_SYSCTL(CTL_POSIX4_PAGESIZE, pagesize);
P4_SYSCTL(CTL_POSIX4_RTSIG_MAX, rtsig_max);
P4_SYSCTL(CTL_POSIX4_SEM_NSEMS_MAX, sem_nsems_max);
P4_SYSCTL(CTL_POSIX4_SEM_VALUE_MAX, sem_value_max);
P4_SYSCTL(CTL_POSIX4_SIGQUEUE_MAX, sigqueue_max);
P4_SYSCTL(CTL_POSIX4_TIMER_MAX, timer_max);

/* posix4_facility: Set a facility to a value.  This is
 * probably a temporary measure until the LKM code is combined with this.
 */
void posix4_facility(int num, int value)
{
	if (num >= 1 && num <= CTL_POSIX4_N_CTLS)
		facility[num - 1] = value;
}
