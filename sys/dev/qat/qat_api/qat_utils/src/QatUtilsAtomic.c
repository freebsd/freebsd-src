/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
#include "qat_utils.h"

#ifdef __x86_64__
__inline int64_t
qatUtilsAtomicGet(QatUtilsAtomic *pAtomicVar)
{
	return ((int64_t)atomic64_read((QatUtilsAtomic *)pAtomicVar));
}

__inline void
qatUtilsAtomicSet(int64_t inValue, QatUtilsAtomic *pAtomicVar)
{
	atomic64_set((QatUtilsAtomic *)pAtomicVar, inValue);
}

__inline int64_t
qatUtilsAtomicAdd(int64_t inValue, QatUtilsAtomic *pAtomicVar)
{
	return atomic64_add_return((long)inValue, (QatUtilsAtomic *)pAtomicVar);
}

__inline int64_t
qatUtilsAtomicSub(int64_t inValue, QatUtilsAtomic *pAtomicVar)
{
	return atomic64_sub_return((long)inValue, (QatUtilsAtomic *)pAtomicVar);
}

__inline int64_t
qatUtilsAtomicInc(QatUtilsAtomic *pAtomicVar)
{
	return atomic64_inc_return((QatUtilsAtomic *)pAtomicVar);
}

__inline int64_t
qatUtilsAtomicDec(QatUtilsAtomic *pAtomicVar)
{
	return atomic64_dec_return((QatUtilsAtomic *)pAtomicVar);
}
#else
__inline int64_t
qatUtilsAtomicGet(QatUtilsAtomic *pAtomicVar)
{
	return ((int64_t)atomic_read((QatUtilsAtomic *)pAtomicVar));
}

__inline void
qatUtilsAtomicSet(int64_t inValue, QatUtilsAtomic *pAtomicVar)
{
	atomic_set((QatUtilsAtomic *)pAtomicVar, inValue);
}

__inline int64_t
qatUtilsAtomicAdd(int64_t inValue, QatUtilsAtomic *pAtomicVar)
{
	return atomic_add_return(inValue, (QatUtilsAtomic *)pAtomicVar);
}

__inline int64_t
qatUtilsAtomicSub(int64_t inValue, QatUtilsAtomic *pAtomicVar)
{
	return atomic_sub_return(inValue, (QatUtilsAtomic *)pAtomicVar);
}

__inline int64_t
qatUtilsAtomicInc(QatUtilsAtomic *pAtomicVar)
{
	return atomic_inc_return((QatUtilsAtomic *)pAtomicVar);
}

__inline int64_t
qatUtilsAtomicDec(QatUtilsAtomic *pAtomicVar)
{
	return atomic_dec_return((QatUtilsAtomic *)pAtomicVar);
}
#endif
