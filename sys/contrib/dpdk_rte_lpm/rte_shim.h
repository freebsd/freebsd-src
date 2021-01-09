#ifndef	_RTE_SHIM_H_
#define	_RTE_SHIM_H_

#define	rte_malloc(_type, _size, _align)	malloc(_size, M_TEMP, M_NOWAIT)
#define	rte_free(_ptr)				free(_ptr, M_TEMP)
#define	rte_zmalloc(_type, _size, _align)	malloc(_size, M_TEMP, M_NOWAIT | M_ZERO)
#define	rte_zmalloc_socket(_type, _size, _align, _s)	malloc(_size, M_TEMP, M_NOWAIT | M_ZERO)

#define	rte_mcfg_tailq_write_unlock()
#define	rte_mcfg_tailq_write_lock()

#define	RTE_CACHE_LINE_SIZE	CACHE_LINE_SIZE
#define strtoull		strtoul
#define	assert(_s)		KASSERT((_s), ("DPDK: assert failed"))
#define	rte_memcpy		memcpy
#define	rte_strerror(_err)	"strerror_not_implemented"
#define	RTE_LOG(_sev, _sub, _fmt, ...)	printf("DPDK::" #_sev "::" #_sub " %s: " _fmt, __func__ , ## __VA_ARGS__)

#include "sys/endian.h"
#define	RTE_BYTE_ORDER	BYTE_ORDER
#define	RTE_LITTLE_ENDIAN	LITTLE_ENDIAN
#define	RTE_BIG_ENDIAN		BIG_ENDIAN

#include "sys/limits.h" // CHAR_BIT
#define	rte_le_to_cpu_32	le32toh

#include "rte_jhash.h"
#include "rte_common.h"


#endif
