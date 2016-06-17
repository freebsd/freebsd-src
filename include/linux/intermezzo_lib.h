/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 * Copyright (C) 2001 Cluster File Systems, Inc. <braam@clusterfs.com>
 *
 *   This file is part of InterMezzo, http://www.inter-mezzo.org.
 *
 *   InterMezzo is free software; you can redistribute it and/or
 *   modify it under the terms of version 2 of the GNU General Public
 *   License as published by the Free Software Foundation.
 *
 *   InterMezzo is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with InterMezzo; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Data structures unpacking/packing macros & inlines
 *
 */

#ifndef _INTERMEZZO_LIB_H
#define _INTERMEZZO_LIB_H

#ifdef __KERNEL__
# include <linux/types.h>
#else
# include <string.h>
# include <sys/types.h>
#endif

#undef MIN
#define MIN(a,b) (((a)<(b)) ? (a): (b))
#undef MAX
#define MAX(a,b) (((a)>(b)) ? (a): (b))
#define MKSTR(ptr) ((ptr))? (ptr) : ""

static inline int size_round (int val)
{
	return (val + 3) & (~0x3);
}

static inline int size_round0(int val)
{
        if (!val) 
                return 0;
	return (val + 1 + 3) & (~0x3);
}

static inline size_t round_strlen(char *fset)
{
	return size_round(strlen(fset) + 1);
}

#ifdef __KERNEL__
# define NTOH__u32(var) le32_to_cpu(var)
# define NTOH__u64(var) le64_to_cpu(var)
# define HTON__u32(var) cpu_to_le32(var)
# define HTON__u64(var) cpu_to_le64(var)
#else
# include <glib.h>
# define NTOH__u32(var) GUINT32_FROM_LE(var)
# define NTOH__u64(var) GUINT64_FROM_LE(var)
# define HTON__u32(var) GUINT32_TO_LE(var)
# define HTON__u64(var) GUINT64_TO_LE(var)
#endif

/* 
 * copy sizeof(type) bytes from pointer to var and move ptr forward.
 * return EFAULT if pointer goes beyond end
 */
#define UNLOGV(var,type,ptr,end)                \
do {                                            \
        var = *(type *)ptr;                     \
        ptr += sizeof(type);                    \
        if (ptr > end )                         \
                return -EFAULT;                 \
} while (0)

/* the following two macros convert to little endian */
/* type MUST be __u32 or __u64 */
#define LUNLOGV(var,type,ptr,end)               \
do {                                            \
        var = NTOH##type(*(type *)ptr);         \
        ptr += sizeof(type);                    \
        if (ptr > end )                         \
                return -EFAULT;                 \
} while (0)

/* now log values */
#define LOGV(var,type,ptr)                      \
do {                                            \
        *((type *)ptr) = var;                   \
        ptr += sizeof(type);                    \
} while (0)

/* and in network order */
#define LLOGV(var,type,ptr)                     \
do {                                            \
        *((type *)ptr) = HTON##type(var);       \
        ptr += sizeof(type);                    \
} while (0)


/* 
 * set var to point at (type *)ptr, move ptr forward with sizeof(type)
 * return from function with EFAULT if ptr goes beyond end
 */
#define UNLOGP(var,type,ptr,end)                \
do {                                            \
        var = (type *)ptr;                      \
        ptr += sizeof(type);                    \
        if (ptr > end )                         \
                return -EFAULT;                 \
} while (0)

#define LOGP(var,type,ptr)                      \
do {                                            \
        memcpy(ptr, var, sizeof(type));         \
        ptr += sizeof(type);                    \
} while (0)

/* 
 * set var to point at (char *)ptr, move ptr forward by size_round(len);
 * return from function with EFAULT if ptr goes beyond end
 */
#define UNLOGL(var,type,len,ptr,end)                    \
do {                                                    \
        if (len == 0)                                   \
                var = (type *)0;                        \
        else {                                          \
                var = (type *)ptr;                      \
                ptr += size_round(len * sizeof(type));  \
        }                                               \
        if (ptr > end )                                 \
                return -EFAULT;                         \
} while (0)

#define UNLOGL0(var,type,len,ptr,end)                           \
do {                                                            \
        UNLOGL(var,type,len+1,ptr,end);                         \
        if ( *((char *)ptr - size_round(len+1) + len) != '\0')  \
                        return -EFAULT;                         \
} while (0)

#define LOGL(var,len,ptr)                               \
do {                                                    \
        size_t __fill = size_round(len);                \
        /* Prevent data leakage. */                     \
        if (__fill > 0)                                 \
                memset((char *)ptr, 0, __fill);         \
        memcpy((char *)ptr, (const char *)var, len);    \
        ptr += __fill;                                  \
} while (0)

#define LOGL0(var,len,ptr)                              \
do {                                                    \
        if (!len) break;                                \
        memcpy((char *)ptr, (const char *)var, len);    \
        *((char *)(ptr) + len) = 0;                     \
        ptr += size_round(len + 1);                     \
} while (0)

#endif /* _INTERMEZZO_LIB_H */

