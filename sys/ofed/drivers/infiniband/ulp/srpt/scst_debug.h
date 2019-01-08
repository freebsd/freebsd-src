/*
 *  include/scst_debug.h
 *
 *  Copyright (C) 2004 - 2009 Vladislav Bolkhovitin <vst@vlnb.net>
 *  Copyright (C) 2004 - 2005 Leonid Stoljar
 *  Copyright (C) 2007 - 2009 ID7 Ltd.
 *
 *  Contains macroses for execution tracing and error reporting
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation, version 2
 *  of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 */

#ifndef __SCST_DEBUG_H
#define __SCST_DEBUG_H

#include <linux/autoconf.h>	/* for CONFIG_* */

#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 19)
#include <linux/bug.h>		/* for WARN_ON_ONCE */
#endif

#if !defined(INSIDE_KERNEL_TREE)
#ifdef CONFIG_SCST_DEBUG

#ifndef CONFIG_DEBUG_BUGVERBOSE
#define sBUG() do {						\
	printk(KERN_CRIT "BUG at %s:%d\n",			\
	       __FILE__, __LINE__);				\
	BUG();							\
} while (0)
#else
#define sBUG() BUG()
#endif

#define sBUG_ON(p) do {						\
	if (unlikely(p)) {					\
		printk(KERN_CRIT "BUG at %s:%d (%s)\n",		\
		       __FILE__, __LINE__, #p);			\
		BUG();						\
	}							\
} while (0)

#else

#define sBUG() BUG()
#define sBUG_ON(p) BUG_ON(p)

#endif
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 18)
#ifndef WARN_ON_ONCE
#define WARN_ON_ONCE(condition) ({				\
	static int __warned;					\
	typeof(condition) __ret_warn_once = (condition);	\
								\
	if (unlikely(__ret_warn_once))				\
		if (!__warned) { 				\
			WARN_ON(1);				\
			__warned = 1;				\
		}						\
	unlikely(__ret_warn_once);				\
})
#endif
#endif

#ifdef CONFIG_SCST_EXTRACHECKS
#define EXTRACHECKS_BUG_ON(a)		sBUG_ON(a)
#define EXTRACHECKS_WARN_ON(a)		WARN_ON(a)
#define EXTRACHECKS_WARN_ON_ONCE(a)	WARN_ON_ONCE(a)
#else
#define EXTRACHECKS_BUG_ON(a)
#define EXTRACHECKS_WARN_ON(a)
#define EXTRACHECKS_WARN_ON_ONCE(a)
#endif

#ifdef CONFIG_SCST_DEBUG
/*#  define LOG_FLAG KERN_DEBUG*/
#  define LOG_FLAG KERN_INFO
#  define INFO_FLAG KERN_INFO
#  define ERROR_FLAG KERN_INFO
#else
# define LOG_FLAG KERN_INFO
# define INFO_FLAG KERN_INFO
# define ERROR_FLAG KERN_ERR
#endif

#define CRIT_FLAG KERN_CRIT

#define NO_FLAG ""

#define TRACE_NULL           0x00000000
#define TRACE_DEBUG          0x00000001
#define TRACE_FUNCTION       0x00000002
#define TRACE_LINE           0x00000004
#define TRACE_PID            0x00000008
#define TRACE_ENTRYEXIT      0x00000010
#define TRACE_BUFF           0x00000020
#define TRACE_MEMORY         0x00000040
#define TRACE_SG_OP          0x00000080
#define TRACE_OUT_OF_MEM     0x00000100
#define TRACE_MINOR          0x00000200 /* less important events */
#define TRACE_MGMT           0x00000400
#define TRACE_MGMT_MINOR     0x00000800
#define TRACE_MGMT_DEBUG     0x00001000
#define TRACE_SCSI           0x00002000
#define TRACE_SPECIAL        0x00004000 /* filtering debug, etc */
#define TRACE_ALL            0xffffffff
/* Flags 0xXXXX0000 are local for users */

#ifndef KERN_CONT
#define KERN_CONT       ""
#endif

/*
 * Note: in the next two printk() statements the KERN_CONT macro is only
 * present to suppress a checkpatch warning (KERN_CONT is defined as "").
 */
#define PRINT(log_flag, format, args...)  \
		printk(KERN_CONT "%s" format "\n", log_flag, ## args)
#define PRINTN(log_flag, format, args...) \
		printk(KERN_CONT "%s" format, log_flag, ## args)

#ifdef LOG_PREFIX
#define __LOG_PREFIX	LOG_PREFIX
#else
#define __LOG_PREFIX	NULL
#endif

#if defined(CONFIG_SCST_DEBUG) || defined(CONFIG_SCST_TRACING)

#ifndef CONFIG_SCST_DEBUG
#define ___unlikely(a)		(a)
#else
#define ___unlikely(a)		unlikely(a)
#endif

extern int debug_print_prefix(unsigned long trace_flag, const char *log_level,
	const char *prefix, const char *func, int line);
extern void debug_print_buffer(const char *log_level, const void *data,
	int len);

#define TRACE(trace, format, args...)					  \
do {									  \
	if (___unlikely(trace_flag & (trace))) {			  \
		char *__tflag = LOG_FLAG;				  \
		if (debug_print_prefix(trace_flag, __tflag, __LOG_PREFIX, \
				       __func__, __LINE__) > 0) {	  \
			__tflag = NO_FLAG;				  \
		}							  \
		PRINT(NO_FLAG, "%s" format, __tflag, args);		  \
	}								  \
} while (0)

#ifdef CONFIG_SCST_DEBUG

#define PRINT_BUFFER(message, buff, len)                            \
do {                                                                \
	PRINT(NO_FLAG, "%s:%s:", __func__, message);		    \
	debug_print_buffer(INFO_FLAG, buff, len);		    \
} while (0)

#else

#define PRINT_BUFFER(message, buff, len)                            \
do {                                                                \
	PRINT(NO_FLAG, "%s:", message);				    \
	debug_print_buffer(INFO_FLAG, buff, len);		    \
} while (0)

#endif

#define PRINT_BUFF_FLAG(flag, message, buff, len)			\
do {									\
	if (___unlikely(trace_flag & (flag))) {				\
		char *__tflag = INFO_FLAG;				\
		if (debug_print_prefix(trace_flag, __tflag, NULL, __func__,\
				       __LINE__) > 0) {			\
			__tflag = NO_FLAG;				\
		}							\
		PRINT(NO_FLAG, "%s%s:", __tflag, message);		\
		debug_print_buffer(INFO_FLAG, buff, len);		\
	}								\
} while (0)

#else  /* CONFIG_SCST_DEBUG || CONFIG_SCST_TRACING */

#define TRACE(trace, args...) do {} while (0)
#define PRINT_BUFFER(message, buff, len) do {} while (0)
#define PRINT_BUFF_FLAG(flag, message, buff, len) do {} while (0)

#endif /* CONFIG_SCST_DEBUG || CONFIG_SCST_TRACING */

#ifdef CONFIG_SCST_DEBUG

#define __TRACE(trace, format, args...)					\
do {									\
	if (trace_flag & (trace)) {					\
		char *__tflag = LOG_FLAG;				\
		if (debug_print_prefix(trace_flag, __tflag, NULL, __func__,\
				       __LINE__) > 0) {			\
			__tflag = NO_FLAG;				\
		}							\
		PRINT(NO_FLAG, "%s" format, __tflag, args);		\
	}								\
} while (0)

#define TRACE_MEM(args...)		__TRACE(TRACE_MEMORY, args)
#define TRACE_SG(args...)		__TRACE(TRACE_SG_OP, args)
#define TRACE_DBG(args...)		__TRACE(TRACE_DEBUG, args)
#define TRACE_DBG_SPECIAL(args...)	__TRACE(TRACE_DEBUG|TRACE_SPECIAL, args)
#define TRACE_MGMT_DBG(args...)		__TRACE(TRACE_MGMT_DEBUG, args)
#define TRACE_MGMT_DBG_SPECIAL(args...)	\
		__TRACE(TRACE_MGMT_DEBUG|TRACE_SPECIAL, args)

#define TRACE_BUFFER(message, buff, len)				\
do {									\
	if (trace_flag & TRACE_BUFF) {					\
		char *__tflag = LOG_FLAG;				\
		if (debug_print_prefix(trace_flag, __tflag, NULL, __func__, \
				       __LINE__) > 0) {			\
			__tflag = NO_FLAG;				\
		}							\
		PRINT(NO_FLAG, "%s%s:", __tflag, message);		\
		debug_print_buffer(LOG_FLAG, buff, len);		\
	}								\
} while (0)

#define TRACE_BUFF_FLAG(flag, message, buff, len)			\
do {									\
	if (trace_flag & (flag)) {					\
		char *__tflag = LOG_FLAG;				\
		if (debug_print_prefix(trace_flag, __tflag, NULL, __func__, \
				       __LINE__) > 0) {			\
			__tflag = NO_FLAG;				\
		}							\
		PRINT(NO_FLAG, "%s%s:", __tflag, message);		\
		debug_print_buffer(LOG_FLAG, buff, len);		\
	}								\
} while (0)

#define PRINT_LOG_FLAG(log_flag, format, args...)			\
do {									\
	char *__tflag = log_flag;					\
	if (debug_print_prefix(trace_flag, __tflag, __LOG_PREFIX,	\
			       __func__, __LINE__) > 0) {		\
		__tflag = NO_FLAG;					\
	}								\
	PRINT(NO_FLAG, "%s" format, __tflag, args);			\
} while (0)

#define PRINT_WARNING(format, args...)					\
do {									\
	if (strcmp(INFO_FLAG, LOG_FLAG)) {				\
		PRINT_LOG_FLAG(LOG_FLAG, "***WARNING*** " format, args); \
	}								\
	PRINT_LOG_FLAG(INFO_FLAG, "***WARNING*** " format, args);	\
} while (0)

#define PRINT_ERROR(format, args...)					\
do {									\
	if (strcmp(ERROR_FLAG, LOG_FLAG)) {				\
		PRINT_LOG_FLAG(LOG_FLAG, "***ERROR*** " format, args);	\
	}								\
	PRINT_LOG_FLAG(ERROR_FLAG, "***ERROR*** " format, args);	\
} while (0)

#define PRINT_CRIT_ERROR(format, args...)				\
do {									\
	/*  if (strcmp(CRIT_FLAG, LOG_FLAG))				\
	    {								\
	    PRINT_LOG_FLAG(LOG_FLAG, "***CRITICAL ERROR*** " format, args); \
	    }*/								\
	PRINT_LOG_FLAG(CRIT_FLAG, "***CRITICAL ERROR*** " format, args); \
} while (0)

#define PRINT_INFO(format, args...)			\
do {							\
	if (strcmp(INFO_FLAG, LOG_FLAG)) {		\
		PRINT_LOG_FLAG(LOG_FLAG, format, args);	\
	}						\
	PRINT_LOG_FLAG(INFO_FLAG, format, args);	\
} while (0)

#define TRACE_ENTRY()							\
do {									\
	if (trace_flag & TRACE_ENTRYEXIT) {				\
		if (trace_flag & TRACE_PID) {				\
			PRINT(LOG_FLAG, "[%d]: ENTRY %s", current->pid, \
				__func__);				\
		}							\
		else {							\
			PRINT(LOG_FLAG, "ENTRY %s", __func__);		\
		}							\
	}								\
} while (0)

#define TRACE_EXIT()							\
do {									\
	if (trace_flag & TRACE_ENTRYEXIT) {				\
		if (trace_flag & TRACE_PID) {				\
			PRINT(LOG_FLAG, "[%d]: EXIT %s", current->pid,	\
				__func__);				\
		}							\
		else {							\
			PRINT(LOG_FLAG, "EXIT %s", __func__);		\
		}							\
	}								\
} while (0)

#define TRACE_EXIT_RES(res)						\
do {									\
	if (trace_flag & TRACE_ENTRYEXIT) {				\
		if (trace_flag & TRACE_PID) {				\
			PRINT(LOG_FLAG, "[%d]: EXIT %s: %ld", current->pid, \
			      __func__, (long)(res));			\
		}							\
		else {							\
			PRINT(LOG_FLAG, "EXIT %s: %ld",			\
				__func__, (long)(res));			\
		}							\
	}                                                               \
} while (0)

#define TRACE_EXIT_HRES(res)						\
do {									\
	if (trace_flag & TRACE_ENTRYEXIT) {				\
		if (trace_flag & TRACE_PID) {				\
			PRINT(LOG_FLAG, "[%d]: EXIT %s: 0x%lx", current->pid, \
			      __func__, (long)(res));			\
		}							\
		else {							\
			PRINT(LOG_FLAG, "EXIT %s: %lx",			\
					__func__, (long)(res));		\
		}							\
	}                                                               \
} while (0)

#else  /* CONFIG_SCST_DEBUG */

#define TRACE_MEM(format, args...) do {} while (0)
#define TRACE_SG(format, args...) do {} while (0)
#define TRACE_DBG(format, args...) do {} while (0)
#define TRACE_DBG_SPECIAL(format, args...) do {} while (0)
#define TRACE_MGMT_DBG(format, args...) do {} while (0)
#define TRACE_MGMT_DBG_SPECIAL(format, args...) do {} while (0)
#define TRACE_BUFFER(message, buff, len) do {} while (0)
#define TRACE_BUFF_FLAG(flag, message, buff, len) do {} while (0)
#define TRACE_ENTRY() do {} while (0)
#define TRACE_EXIT() do {} while (0)
#define TRACE_EXIT_RES(res) do {} while (0)
#define TRACE_EXIT_HRES(res) do {} while (0)

#ifdef LOG_PREFIX

#define PRINT_INFO(format, args...)				\
do {								\
	PRINT(INFO_FLAG, "%s: " format, LOG_PREFIX, args);	\
} while (0)

#define PRINT_WARNING(format, args...)          \
do {                                            \
	PRINT(INFO_FLAG, "%s: ***WARNING*** "	\
	      format, LOG_PREFIX, args);	\
} while (0)

#define PRINT_ERROR(format, args...)            \
do {                                            \
	PRINT(ERROR_FLAG, "%s: ***ERROR*** "	\
	      format, LOG_PREFIX, args);	\
} while (0)

#define PRINT_CRIT_ERROR(format, args...)       \
do {                                            \
	PRINT(CRIT_FLAG, "%s: ***CRITICAL ERROR*** "	\
		format, LOG_PREFIX, args);		\
} while (0)

#else

#define PRINT_INFO(format, args...)           	\
do {                                            \
	PRINT(INFO_FLAG, format, args);		\
} while (0)

#define PRINT_WARNING(format, args...)          \
do {                                            \
	PRINT(INFO_FLAG, "***WARNING*** "	\
		format, args);			\
} while (0)

#define PRINT_ERROR(format, args...)          	\
do {                                            \
	PRINT(ERROR_FLAG, "***ERROR*** "	\
		format, args);			\
} while (0)

#define PRINT_CRIT_ERROR(format, args...)		\
do {							\
	PRINT(CRIT_FLAG, "***CRITICAL ERROR*** "	\
		format, args);				\
} while (0)

#endif /* LOG_PREFIX */

#endif /* CONFIG_SCST_DEBUG */

#if defined(CONFIG_SCST_DEBUG) && defined(CONFIG_DEBUG_SLAB)
#define SCST_SLAB_FLAGS (SLAB_RED_ZONE | SLAB_POISON)
#else
#define SCST_SLAB_FLAGS 0L
#endif

#endif /* __SCST_DEBUG_H */
