# $FreeBSD$

BIND_DIR=	${.CURDIR}/../../contrib/bind

LIB=		isc
SHLIB_MAJOR=	1
SHLIB_MINOR=	0

.PATH:		${BIND_DIR}/lib/isc
SRCS=		assertions.c ev_connects.c ev_files.c ev_streams.c \
		ev_timers.c ev_waits.c eventlib.c heap.c hex.c tree.c \
		logging.c memcluster.c bitncmp.c ctl_clnt.c ctl_p.c \
		ctl_srvr.c movefile.c

# contrib/bind/include/* must not override any real system includes
CFLAGS+=	-I${BIND_DIR}/port/freebsd/include -I${BIND_DIR}/include
CFLAGS+=	-Wall -Wmissing-prototypes

INCS=		${BIND_DIR}/include/isc/assertions.h \
		${BIND_DIR}/include/isc/ctl.h \
		${BIND_DIR}/include/isc/dst.h \
		${BIND_DIR}/include/isc/eventlib.h \
		${BIND_DIR}/include/isc/heap.h \
		${BIND_DIR}/include/isc/irpmarshall.h \
		${BIND_DIR}/include/isc/list.h \
		${BIND_DIR}/include/isc/logging.h \
		${BIND_DIR}/include/isc/memcluster.h \
		${BIND_DIR}/include/isc/misc.h \
		${BIND_DIR}/include/isc/tree.h
INCSDIR=	${INCLUDEDIR}/isc

MAN=		assertions.3 bitncmp.3 eventlib.3 heap.3 \
		logging.3 memcluster.3 tree.3
CLEANFILES+=	${MAN}

MLINKS+=	assertions.3 ENSURE.3 \
		assertions.3 ENSURE_ERR.3 \
		assertions.3 INSIST.3 \
		assertions.3 INSIST_ERR.3 \
		assertions.3 INVARIANT.3 \
		assertions.3 INVARIANT_ERR.3 \
		assertions.3 REQUIRE.3 \
		assertions.3 REQUIRE_ERR.3 \
		assertions.3 assertion_type_to_text.3 \
		assertions.3 set_assertion_failure_callback.3 \
		eventlib.3 evAddTime.3 \
		eventlib.3 evCancelConn.3 \
		eventlib.3 evCancelRW.3 \
		eventlib.3 evClearIdleTimer.3 \
		eventlib.3 evClearTimer.3 \
		eventlib.3 evCmpTime.3 \
		eventlib.3 evConnFunc.3 \
		eventlib.3 evConnect.3 \
		eventlib.3 evConsIovec.3 \
		eventlib.3 evConsTime.3 \
		eventlib.3 evCreate.3 \
		eventlib.3 evDefer.3 \
		eventlib.3 evDeselectFD.3 \
		eventlib.3 evDestroy.3 \
		eventlib.3 evDispatch.3 \
		eventlib.3 evDo.3 \
		eventlib.3 evDrop.3 \
		eventlib.3 evFileFunc.3 \
		eventlib.3 evGetNext.3 \
		eventlib.3 evHold.3 \
		eventlib.3 evInitID.3 \
		eventlib.3 evLastEventTime.3 \
		eventlib.3 evListen.3 \
		eventlib.3 evMainLoop.3 \
		eventlib.3 evNowTime.3 \
		eventlib.3 evPrintf.3 \
		eventlib.3 evRead.3 \
		eventlib.3 evResetTimer.3 \
		eventlib.3 evSelectFD.3 \
		eventlib.3 evSetDebug.3 \
		eventlib.3 evSetIdleTimer.3 \
		eventlib.3 evSetTimer.3 \
		eventlib.3 evStreamFunc.3 \
		eventlib.3 evSubTime.3 \
		eventlib.3 evTestID.3 \
		eventlib.3 evTimeRW.3 \
		eventlib.3 evTimeSpec.3 \
		eventlib.3 evTimeVal.3 \
		eventlib.3 evTimerFunc.3 \
		eventlib.3 evTouchIdleTimer.3 \
		eventlib.3 evTryAccept.3 \
		eventlib.3 evUnhold.3 \
		eventlib.3 evUntimeRW.3 \
		eventlib.3 evUnwait.3 \
		eventlib.3 evWaitFor.3 \
		eventlib.3 evWaitFunc.3 \
		eventlib.3 evWrite.3 \
		heap.3 heap_decreased.3 \
		heap.3 heap_delete.3 \
		heap.3 heap_element.3 \
		heap.3 heap_for_each.3 \
		heap.3 heap_free.3 \
		heap.3 heap_increased.3 \
		heap.3 heap_insert.3 \
		heap.3 heap_new.3 \
		logging.3 log_add_channel.3 \
		logging.3 log_category_is_active.3 \
		logging.3 log_close_stream.3 \
		logging.3 log_dec_references.3 \
		logging.3 log_free_channel.3 \
		logging.3 log_free_context.3 \
		logging.3 log_get_filename.3 \
		logging.3 log_get_stream.3 \
		logging.3 log_inc_references.3 \
		logging.3 log_new_context.3 \
		logging.3 log_new_file_channel.3 \
		logging.3 log_new_null_channel.3 \
		logging.3 log_new_syslog_channel.3 \
		logging.3 log_open_stream.3 \
		logging.3 log_option.3 \
		logging.3 log_remove_channel.3 \
		logging.3 log_set_file_owner.3 \
		logging.3 log_vwrite.3 \
		logging.3 log_write.3 \
		memcluster.3 memget.3 \
		memcluster.3 memput.3 \
		memcluster.3 memstats.3 \
		tree.3 tree_add.3 \
		tree.3 tree_delete.3 \
		tree.3 tree_init.3 \
		tree.3 tree_mung.3 \
		tree.3 tree_srch.3 \
		tree.3 tree_trav.3

.include <bsd.lib.mk>

# This has to come after the .include <bsd.lib.mk>. Not sure why.

.SUFFIXES:	.mdoc .3
.mdoc.3:
		sed -e 's/@INDOT@//g' \
		    -e 's/@SYSCALL_EXT@/2/g' \
		    -e 's/@SYS_OPS_EXT@/8/g' \
		    < ${.ALLSRC} > ${.TARGET}

