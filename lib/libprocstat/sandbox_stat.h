/*-
 * Copyright (c) 2013-2014 Robert N. M. Watson
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _SANDBOX_STAT_H_
#define	_SANDBOX_STAT_H_

/*
 * APIs and definitions allowing inspection of sandbox state from outside of
 * the process using sysctl.
 *
 * NB: These are quite ABI-sensitive, don't change their length or layout, and
 * consider carefully the effects of changes on padding on both 32-bit and
 * 64-bit systems.  Padding has been provided for (bounded) future growth.
 */

#define	SANDBOX_CLASSID_FREE	0	/* Class array slot is unallocated. */
#define	SANDBOX_METHODID_FREE	0	/* Method array slot is unallocated. */
#define	SANDBOX_OBJECTID_FREE	0	/* Object array slot is unallocated. */

#define	SANDBOX_CLASS_MAXNAMELEN	32
#define	SANDBOX_METHOD_MAXNAMELEN	32
#define	SANDBOX_OBJECT_MAXNAMELEN	32

#define	SANDBOX_METHOD_MAXSAMPVEC	8
#define	SANDBOX_OBJECT_MAXSAMPVEC	8

/*
 * Per-sandbox class description.
 */
struct sandbox_class_stat {
	char	scs_class_name[SANDBOX_CLASS_MAXNAMELEN];
	uint64_t	scs_classid;		/* Unique class identifier. */
	uint64_t	scs_pad0[8];		/* Future growth. */

	/*
	 * Statistics.
	 */
	uint64_t	scs_stat_alloc;		/* Number allocated to date. */
	uint64_t	scs_stat_reset;		/* Number of resets to date. */
	uint64_t	scs_stat_free;		/* Number freed to date. */
	uint64_t	scs_pad1[8];		/* Future growth. */
};

/*
 * Macros to bump various sandbox-class statistics.
 *
 * XXXRW: Should these use atomic(9)?
 */
#define	SANDBOX_CLASS_ALLOC(scsp)	do {				\
	if ((scsp) != NULL)						\
		(scsp)->scs_stat_alloc++;				\
} while (0)

#define	SANDBOX_CLASS_RESET(scsp)	do {				\
	if ((scsp) != NULL)						\
		(scsp)->scs_stat_reset++;				\
} while (0)

#define	SANDBOX_CLASS_FREE(scsp)	do {				\
	if ((scsp) != NULL)						\
		(scsp)->scs_stat_free++;				\
} while (0)

/*
 * Per-sandbox method description.
 */
struct sandbox_method_stat {
	char	sms_method_name[SANDBOX_METHOD_MAXNAMELEN];

	/* Cache a copy of the class name here for convenience to procstat. */
	char	sms_class_name[SANDBOX_CLASS_MAXNAMELEN];
	uint64_t	sms_classid;		/* Associated class. */
	uint64_t	sms_methodid;		/* Per-class unique method. */
	uint64_t	sms_pad0[8];		/* Future growth. */

	/*
	 * Statistics.
	 */
	uint64_t	sms_stat_invoke;	/* Number of invocations. */
	uint64_t	sms_stat_fault;		/* Number of faulted aborts. */
	uint64_t	sms_stat_timeout;	/* Number of timeouts. */
	uint64_t	sms_stat_minrun;	/* Minimum invoke time. */
	uint64_t	sms_stat_maxrun;	/* Maximum invoke time. */
	uint64_t	sms_stat_nextidx;	/* Next index into vector. */
	uint64_t	sms_stat_sampvec[SANDBOX_METHOD_MAXSAMPVEC];
						/* Time sample window. */
	uint64_t	sms_pad1[1];		/* Future growth. */
};

/*
 * Macros to bump various sandbox-method statistics.
 *
 * XXXRW: Not all defined.
 *
 * XXXRW: Should these use atomic(9)?
 */
#define	SANDBOX_METHOD_INVOKE(smsp)	do {				\
	if ((smsp) != NULL)						\
		(smsp)->sms_stat_invoke++;				\
} while (0)

#define	SANDBOX_METHOD_FAULT(smsp)	do {				\
	if ((smsp) != NULL)						\
		(smsp)->sms_stat_fault++;				\
} while (0)

#define	SANDBOX_METHOD_TIMEOUT(smsp)	do {				\
	if ((smsp) != NULL)						\
		(smsp)->sms_stat_timeout++;				\
} while (0)

#define	SANDBOX_METHOD_TIME_SAMPLE(smsp, sample)	do {		\
	if ((smsp) != NULL) {						\
		(smsp)->sms_stat_sampvec[(smsp)->sms_stat_nextidx++ %	\
		    SANDBOX_METHOD_MAXSAMPVEC];				\
		if ((sample) < (smsp)->sms_stat_minrun ||		\
		    (smsp)->sms_stat_minrun == 0)			\
			(smsp)->sms_stat_minrun = (sample);		\
		if ((sample) > (smsp)->sms_stat_maxrun)			\
			(smsp)->sms_stat_maxrun = (sample);		\
	}								\
} while (0)

/*
 * Per-sandbox object description.
 */
#define	SANDBOX_OBJECT_TYPE_PID		1
#define	SANDBOX_OBJECT_TYPE_POINTER	2
struct sandbox_object_stat {
	char	sos_class_name[SANDBOX_CLASS_MAXNAMELEN];
	uint64_t	sos_classid;		/* Associated class. */
	uint64_t	sos_objectid;		/* Per-class unique object. */
	uint64_t	sos_object_type;	/* PID, pointer, etc. */
	uint64_t	sos_object_name;	/* Model-specific name.  E.g.,
						 * 64-bit pointer or PID. */
	uint64_t	sos_pad0[8];		/* Future growth. */
	/*
	 * Statistics.
	 */
	uint64_t	sos_stat_invoke;	/* Number of invocations. */
	uint64_t	sos_stat_fault;		/* Number of faulted aborts. */
	uint64_t	sos_stat_timeout;	/* Number of timeouts. */
	uint64_t	sos_stat_minrun;	/* Minimum invoke time. */
	uint64_t	sos_stat_maxrun;	/* Maximum invoke time. */
	uint64_t	sos_stat_nextidx;	/* Next index into vector. */
	uint64_t	sos_stat_sampvec[SANDBOX_OBJECT_MAXSAMPVEC];
						/* Time sample window. */
	uint64_t	sos_pad1[1];		/* Future growth. */
};

/*
 * Macros to bump various sandbox-method statistics.
 *
 * XXXRW: Not all defined.
 *
 * XXXRW: Should these use atomic(9)?
 */
#define	SANDBOX_OBJECT_INVOKE(sosp)	do {				\
	if ((sosp) != NULL)						\
		(sosp)->sos_stat_invoke++;				\
} while (0)

#define	SANDBOX_OBJECT_FAULT(sosp)	do {				\
	if ((sosp) != NULL)						\
		(sosp)->sos_stat_fault++;				\
} while (0)

#define	SANDBOX_OBJECT_TIME_SAMPLE(sosp, sample)	do {		\
	if ((sosp) != NULL) {						\
		(sosp)->sos_stat_sampvec[(sosp)->sos_stat_nextidx++ %	\
		    SANDBOX_OBJECT_MAXSAMPVEC];				\
		if ((sample) < (sosp)->sos_stat_minrun ||		\
		    (sosp)->sos_stat_minrun == 0)			\
			(sosp)->sos_stat_minrun = (sample);		\
		if ((sample) > (sosp)->sos_stat_maxrun)			\
			(sosp)->sos_stat_maxrun = (sample);		\
	}								\
} while (0)

/*
 * Interfaces to register/deregister classes and methods.
 */
int	sandbox_stat_class_register(struct sandbox_class_stat **scspp,
	    const char *name);
void	sandbox_stat_class_deregister(struct sandbox_class_stat *scsp);
int	sandbox_stat_method_register(struct sandbox_method_stat **smspp,
	    struct sandbox_class_stat *scsp, const char *name);
void	sandbox_stat_method_deregister(struct sandbox_method_stat *smsp);
int	sandbox_stat_object_register(struct sandbox_object_stat **sospp,
	    struct sandbox_class_stat *scsp, uint64_t type, uint64_t name);
void	sandbox_stat_object_deregister(struct sandbox_object_stat *sosp);

#endif /* !_SANDBOX_STAT_H_ */
