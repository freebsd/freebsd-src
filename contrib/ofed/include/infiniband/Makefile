.include <bsd.own.mk>

INCS=
IBVERBS=	../../libibverbs/include/infiniband
VERBINCS=	${IBVERBS}/arch.h ${IBVERBS}/driver.h ${IBVERBS}/kern-abi.h
VERBINCS+=	${IBVERBS}/marshall.h ${IBVERBS}/opcode.h
VERBINCS+=	${IBVERBS}/sa-kern-abi.h ${IBVERBS}/sa.h ${IBVERBS}/verbs.h
VERBINCSDIR=	${INCLUDEDIR}/infiniband

IBCOMMON=	../../management/libibcommon/include/infiniband
COMMONINCS=	${IBCOMMON}/common.h
COMMONINCSDIR=	${INCLUDEDIR}/infiniband

IBMAD=		../../management/libibmad/include/infiniband
MADINCS=	${IBMAD}/mad.h
MADINCSDIR=	${INCLUDEDIR}/infiniband

IBUMAD=		../../management/libibumad/include/infiniband
UMADINCS=	${IBUMAD}/umad.h
UMADINCSDIR=	${INCLUDEDIR}/infiniband

COMPLIB=	../../management/opensm/include/complib
COMPLIBINCS=	${COMPLIB}/cl_atomic.h ${COMPLIB}/cl_atomic_osd.h
COMPLIBINCS+= 	${COMPLIB}/cl_byteswap.h ${COMPLIB}/cl_byteswap_osd.h
COMPLIBINCS+= 	${COMPLIB}/cl_comppool.h ${COMPLIB}/cl_debug.h
COMPLIBINCS+=	${COMPLIB}/cl_debug_osd.h ${COMPLIB}/cl_dispatcher.h
COMPLIBINCS+=	${COMPLIB}/cl_event.h ${COMPLIB}/cl_event_osd.h
COMPLIBINCS+=	${COMPLIB}/cl_event_wheel.h ${COMPLIB}/cl_fleximap.h
COMPLIBINCS+=	${COMPLIB}/cl_list.h ${COMPLIB}/cl_log.h
COMPLIBINCS+=	${COMPLIB}/cl_map.h ${COMPLIB}/cl_math.h
COMPLIBINCS+=	${COMPLIB}/cl_nodenamemap.h ${COMPLIB}/cl_packoff.h
COMPLIBINCS+=	${COMPLIB}/cl_packon.h ${COMPLIB}/cl_passivelock.h
COMPLIBINCS+=	${COMPLIB}/cl_pool.h ${COMPLIB}/cl_ptr_vector.h
COMPLIBINCS+=	${COMPLIB}/cl_qcomppool.h ${COMPLIB}/cl_qlist.h
COMPLIBINCS+=	${COMPLIB}/cl_qmap.h ${COMPLIB}/cl_qpool.h
COMPLIBINCS+=	${COMPLIB}/cl_spinlock.h ${COMPLIB}/cl_spinlock_osd.h
COMPLIBINCS+=	${COMPLIB}/cl_thread.h ${COMPLIB}/cl_thread_osd.h
COMPLIBINCS+=	${COMPLIB}/cl_threadpool.h ${COMPLIB}/cl_timer.h
COMPLIBINCS+=	${COMPLIB}/cl_timer_osd.h ${COMPLIB}/cl_types.h
COMPLIBINCS+=	${COMPLIB}/cl_types_osd.h ${COMPLIB}/cl_vector.h
COMPLIBINCSDIR=	${INCLUDEDIR}/infiniband/complib

IBADIR=		../../management/opensm/include/iba
IBAINCS=	${IBADIR}/ib_cm_types.h ${IBADIR}/ib_types.h
IBAINCSDIR=	${INCLUDEDIR}/infiniband/iba

INCSGROUPS=	INCS VERBINCS COMMONINCS MADINCS UMADINCS COMPLIBINCS IBAINCS

.include <bsd.prog.mk>
