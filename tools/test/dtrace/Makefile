# $FreeBSD$

TESTSRCDIR=	${.CURDIR}/../../../cddl/contrib/opensolaris/cmd/dtrace/test
CFLAGS+=	-w
DTEST=		${TESTSRCDIR}/cmd/scripts/dtest.pl -n

IGNORE=		\
	${TESTSRCDIR}/tst/common/aggs/tst.subr.d			\
	${TESTSRCDIR}/tst/common/buffering/tst.cputime.ksh		\
	${TESTSRCDIR}/tst/common/buffering/tst.ring3.d			\
	${TESTSRCDIR}/tst/common/cpc/err.D_PDESC_ZERO.lowfrequency.d	\
	${TESTSRCDIR}/tst/common/cpc/err.D_PDESC_ZERO.malformedoverflow.d\
	${TESTSRCDIR}/tst/common/cpc/err.D_PDESC_ZERO.nonexistentevent.d\
	${TESTSRCDIR}/tst/common/cpc/err.cpcvscpustatpart1.ksh		\
	${TESTSRCDIR}/tst/common/cpc/err.cpcvscpustatpart2.ksh		\
	${TESTSRCDIR}/tst/common/cpc/err.cputrackfailtostart.ksh	\
	${TESTSRCDIR}/tst/common/cpc/err.cputrackterminates.ksh		\
	${TESTSRCDIR}/tst/common/cpc/err.toomanyenablings.d		\
	${TESTSRCDIR}/tst/common/cpc/tst.allcpus.ksh			\
	${TESTSRCDIR}/tst/common/cpc/tst.genericevent.d			\
	${TESTSRCDIR}/tst/common/cpc/tst.platformevent.ksh		\
	${TESTSRCDIR}/tst/common/dtraceUtil/man.ListProbesWithFunctions	\
	${TESTSRCDIR}/tst/common/dtraceUtil/man.ListProbesWithModules	\
	${TESTSRCDIR}/tst/common/dtraceUtil/man.ListProbesWithNames	\
	${TESTSRCDIR}/tst/common/dtraceUtil/man.ListProbesWithProviders	\
	${TESTSRCDIR}/tst/common/dtraceUtil/man.TraceFunctions		\
	${TESTSRCDIR}/tst/common/dtraceUtil/man.TraceModule		\
	${TESTSRCDIR}/tst/common/dtraceUtil/man.TraceNames		\
	${TESTSRCDIR}/tst/common/dtraceUtil/man.TraceProvider		\
	${TESTSRCDIR}/tst/common/dtraceUtil/tst.InvalidModule3.d.ksh	\
	${TESTSRCDIR}/tst/common/dtraceUtil/tst.InvalidProvider3.d.ksh	\
	${TESTSRCDIR}/tst/common/dtraceUtil/tst.InvalidTraceFunc7.d.ksh	\
	${TESTSRCDIR}/tst/common/dtraceUtil/tst.InvalidTraceModule6.d.ksh\
	${TESTSRCDIR}/tst/common/dtraceUtil/tst.InvalidTraceProvider4.d.ksh\
	${TESTSRCDIR}/tst/common/funcs/tst.bcopy.d			\
	${TESTSRCDIR}/tst/common/funcs/tst.copyin.d			\
	${TESTSRCDIR}/tst/common/funcs/tst.copyinto.d			\
	${TESTSRCDIR}/tst/common/funcs/tst.ddi_pathname.d		\
	${TESTSRCDIR}/tst/common/funcs/tst.index.d			\
	${TESTSRCDIR}/tst/common/inline/err.D_OP_INCOMPAT.badxlate.d	\
	${TESTSRCDIR}/tst/common/inline/tst.InlineTypedef.d		\
	${TESTSRCDIR}/tst/common/io/tst.fds.c				\
	${TESTSRCDIR}/tst/common/java_api/tst.FunctionLookup.c		\
	${TESTSRCDIR}/tst/common/java_api/tst.ProbeData.c		\
	${TESTSRCDIR}/common/misc/tst.include.ksh			\
	${TESTSRCDIR}/tst/common/misc/tst.roch.d			\
	${TESTSRCDIR}/tst/common/nfs/tst.call.c				\
	${TESTSRCDIR}/tst/common/nfs/tst.call.d				\
	${TESTSRCDIR}/tst/common/nfs/tst.call3.c			\
	${TESTSRCDIR}/tst/common/nfs/tst.call3.d			\
	${TESTSRCDIR}/tst/common/offsetof/err.D_OFFSETOF_BITFIELD.bitfield.d\
	${TESTSRCDIR}/tst/common/pointers/err.D_OP_LVAL.AddressChange.d	\
	${TESTSRCDIR}/tst/common/pointers/tst.GlobalVar.d		\
	${TESTSRCDIR}/tst/common/pointers/tst.basic1.d			\
	${TESTSRCDIR}/tst/common/pointers/tst.basic2.d			\
	${TESTSRCDIR}/tst/common/predicates/tst.argsnotcached.d		\
	${TESTSRCDIR}/tst/common/privs/tst.func_access.ksh		\
	${TESTSRCDIR}/tst/common/privs/tst.op_access.ksh		\
	${TESTSRCDIR}/tst/common/proc/tst.discard.ksh			\
	${TESTSRCDIR}/tst/common/proc/tst.signal.ksh			\
	${TESTSRCDIR}/tst/common/proc/tst.startexit.ksh			\
	${TESTSRCDIR}/tst/common/scalars/tst.misc.d			\
	${TESTSRCDIR}/tst/common/scalars/tst.selfarray2.d		\
	${TESTSRCDIR}/tst/common/sysevent/tst.post.c			\
	${TESTSRCDIR}/tst/common/sysevent/tst.post.d			\
	${TESTSRCDIR}/tst/common/sysevent/tst.post_chan.c		\
	${TESTSRCDIR}/tst/common/sysevent/tst.post_chan.d		\
	${TESTSRCDIR}/tst/common/trace/tst.misc.d			\
	${TESTSRCDIR}/tst/common/tracemem/err.D_TRACEMEM_ADDR.badaddr.d	\
	${TESTSRCDIR}/tst/common/tracemem/tst.rootvp.d			\
	${TESTSRCDIR}/tst/common/translators/tst.ProcModelTrans.d	\
	${TESTSRCDIR}/tst/common/types/err.D_XLATE_REDECL.ResultDynType.d\
	${TESTSRCDIR}/tst/common/types/tst.ptrops.d			\
	${TESTSRCDIR}/tst/common/types/tst.struct.d			\
	${TESTSRCDIR}/tst/common/types/tst.typedef.d			\
	${TESTSRCDIR}/tst/common/vars/tst.ucaller.ksh			\
	${TESTSRCDIR}/tst/common/vars/tst.walltimestamp.d		\
	${TESTSRCDIR}/tst/common/usdt/tst.argmap.c			\
	${TESTSRCDIR}/tst/common/usdt/tst.args.c			\
	${TESTSRCDIR}/tst/common/usdt/tst.forker.c			\
	${TESTSRCDIR}/tst/common/ip/tst.ipv4localtcp.ksh		\
	${TESTSRCDIR}/tst/common/ip/tst.ipv4localudp.ksh		\
	${TESTSRCDIR}/tst/common/ip/tst.ipv4remotetcp.ksh		\
	${TESTSRCDIR}/tst/common/ip/tst.ipv4remoteudp.ksh		\
	${TESTSRCDIR}/tst/common/ip/tst.localtcpstate.ksh		\
	${TESTSRCDIR}/tst/common/ip/tst.remotetcpstate.ksh		\
	${TESTSRCDIR}/tst/common/scripting/tst.projid.ksh		\
	${TESTSRCDIR}/tst/common/scripting/tst.taskid.ksh



NOTWORK+= \
	${TESTSRCDIR}/tst/common/io/tst.fds.d				\
	${TESTSRCDIR}/tst/common/java_api/tst.Abort.ksh			\
	${TESTSRCDIR}/tst/common/java_api/tst.Bean.ksh			\
	${TESTSRCDIR}/tst/common/java_api/tst.Close.ksh			\
	${TESTSRCDIR}/tst/common/java_api/tst.Drop.ksh			\
	${TESTSRCDIR}/tst/common/java_api/tst.Enable.ksh		\
	${TESTSRCDIR}/tst/common/java_api/tst.FunctionLookup.ksh	\
	${TESTSRCDIR}/tst/common/java_api/tst.GetAggregate.ksh		\
	${TESTSRCDIR}/tst/common/java_api/tst.MaxConsumers.ksh		\
	${TESTSRCDIR}/tst/common/java_api/tst.MultiAggPrinta.ksh	\
	${TESTSRCDIR}/tst/common/java_api/tst.ProbeData.ksh		\
	${TESTSRCDIR}/tst/common/java_api/tst.ProbeDescription.ksh	\
	${TESTSRCDIR}/tst/common/java_api/tst.StateMachine.ksh		\
	${TESTSRCDIR}/tst/common/java_api/tst.StopLock.ksh 		\
	${TESTSRCDIR}/tst/common/java_api/tst.printa.d			\
	${TESTSRCDIR}/tst/common/lexer/err.D_CHR_NL.char.d		\
	${TESTSRCDIR}/tst/common/lexer/err.D_STR_NL.string.d		\
	${TESTSRCDIR}/tst/common/mdb/tst.dtracedcmd.ksh			\
	${TESTSRCDIR}/tst/common/mib/tst.icmp.ksh			\
	${TESTSRCDIR}/tst/common/mib/tst.tcp.ksh			\
	${TESTSRCDIR}/tst/common/mib/tst.udp.ksh			\
	${TESTSRCDIR}/tst/common/profile-n/tst.func.ksh			\
	${TESTSRCDIR}/tst/common/profile-n/tst.mod.ksh			\
	${TESTSRCDIR}/tst/common/profile-n/tst.sym.ksh			\
	${TESTSRCDIR}/tst/common/safety/tst.basename.d			\
	${TESTSRCDIR}/tst/common/safety/tst.caller.d			\
	${TESTSRCDIR}/tst/common/safety/tst.cleanpath.d			\
	${TESTSRCDIR}/tst/common/safety/tst.copyin.d			\
	${TESTSRCDIR}/tst/common/safety/tst.copyin2.d			\
	${TESTSRCDIR}/tst/common/safety/tst.ddi_pathname.d		\
	${TESTSRCDIR}/tst/common/safety/tst.dirname.d			\
	${TESTSRCDIR}/tst/common/safety/tst.errno.d			\
	${TESTSRCDIR}/tst/common/safety/tst.execname.d			\
	${TESTSRCDIR}/tst/common/safety/tst.gid.d			\
	${TESTSRCDIR}/tst/common/safety/tst.hton.d			\
	${TESTSRCDIR}/tst/common/safety/tst.index.d			\
	${TESTSRCDIR}/tst/common/safety/tst.msgdsize.d			\
	${TESTSRCDIR}/tst/common/safety/tst.msgsize.d			\
	${TESTSRCDIR}/tst/common/safety/tst.null.d			\
	${TESTSRCDIR}/tst/common/safety/tst.pid.d			\
	${TESTSRCDIR}/tst/common/safety/tst.ppid.d			\
	${TESTSRCDIR}/tst/common/safety/tst.progenyof.d			\
	${TESTSRCDIR}/tst/common/safety/tst.random.d			\
	${TESTSRCDIR}/tst/common/safety/tst.rw.d			\
	${TESTSRCDIR}/tst/common/safety/tst.shortstr.d			\
	${TESTSRCDIR}/tst/common/safety/tst.stack.d			\
	${TESTSRCDIR}/tst/common/safety/tst.stackdepth.d		\
	${TESTSRCDIR}/tst/common/safety/tst.stddev.d			\
	${TESTSRCDIR}/tst/common/safety/tst.strchr.d			\
	${TESTSRCDIR}/tst/common/safety/tst.strjoin.d			\
	${TESTSRCDIR}/tst/common/safety/tst.strstr.d			\
	${TESTSRCDIR}/tst/common/safety/tst.strtok.d			\
	${TESTSRCDIR}/tst/common/safety/tst.substr.d			\
	${TESTSRCDIR}/tst/common/safety/tst.ucaller.d			\
	${TESTSRCDIR}/tst/common/safety/tst.uid.d			\
	${TESTSRCDIR}/tst/common/safety/tst.unalign.d			\
	${TESTSRCDIR}/tst/common/safety/tst.ustack.d			\
	${TESTSRCDIR}/tst/common/safety/tst.ustackdepth.d		\
	${TESTSRCDIR}/tst/common/safety/tst.vahole.d			\
	${TESTSRCDIR}/tst/common/safety/tst.violentdeath.ksh		\
	${TESTSRCDIR}/tst/common/safety/tst.zonename.d			\
	${TESTSRCDIR}/tst/common/sched/tst.enqueue.d			\
	${TESTSRCDIR}/tst/common/sched/tst.oncpu.d			\
	${TESTSRCDIR}/tst/common/sched/tst.stackdepth.d			\
	${TESTSRCDIR}/tst/common/usdt/argmap.d				\
	${TESTSRCDIR}/tst/common/usdt/args.d				\
	${TESTSRCDIR}/tst/common/usdt/forker.d				\
	${TESTSRCDIR}/tst/common/usdt/tst.andpid.ksh			\
	${TESTSRCDIR}/tst/common/usdt/tst.argmap.d			\
	${TESTSRCDIR}/tst/common/usdt/tst.args.d			\
	${TESTSRCDIR}/tst/common/usdt/tst.badguess.ksh			\
	${TESTSRCDIR}/tst/common/usdt/tst.dlclose1.ksh			\
	${TESTSRCDIR}/tst/common/usdt/tst.dlclose2.ksh			\
	${TESTSRCDIR}/tst/common/usdt/tst.dlclose3.ksh			\
	${TESTSRCDIR}/tst/common/usdt/tst.eliminate.ksh			\
	${TESTSRCDIR}/tst/common/usdt/tst.enabled.ksh			\
	${TESTSRCDIR}/tst/common/usdt/tst.entryreturn.ksh		\
	${TESTSRCDIR}/tst/common/usdt/tst.fork.ksh			\
	${TESTSRCDIR}/tst/common/usdt/tst.forker.ksh			\
	${TESTSRCDIR}/tst/common/usdt/tst.guess32.ksh			\
	${TESTSRCDIR}/tst/common/usdt/tst.guess64.ksh			\
	${TESTSRCDIR}/tst/common/usdt/tst.header.ksh			\
	${TESTSRCDIR}/tst/common/usdt/tst.include.ksh			\
	${TESTSRCDIR}/tst/common/usdt/tst.linkpriv.ksh			\
	${TESTSRCDIR}/tst/common/usdt/tst.linkunpriv.ksh		\
	${TESTSRCDIR}/tst/common/usdt/tst.multiple.ksh			\
	${TESTSRCDIR}/tst/common/usdt/tst.nodtrace.ksh			\
	${TESTSRCDIR}/tst/common/usdt/tst.onlyenabled.ksh		\
	${TESTSRCDIR}/tst/common/usdt/tst.reeval.ksh			\
	${TESTSRCDIR}/tst/common/usdt/tst.static.ksh			\
	${TESTSRCDIR}/tst/common/usdt/tst.static2.ksh			\
	${TESTSRCDIR}/tst/common/usdt/tst.user.ksh			\
	${TESTSRCDIR}/tst/common/misc/tst.schrock.ksh 

# --------------------------------------------------------------------------------
# Tests that just don't complete (even to fail) at the moment...

NOTWORK+= \
	${TESTSRCDIR}/tst/common/dtraceUtil/tst.ZeroModuleProbes.d.ksh	\
	${TESTSRCDIR}/tst/common/printa/tst.many.d			\
	${TESTSRCDIR}/tst/common/scalars/err.D_OP_INCOMPAT.dupgtype.d	\
	${TESTSRCDIR}/tst/common/scalars/err.D_OP_INCOMPAT.dupltype.d	\
	${TESTSRCDIR}/tst/common/scalars/err.D_OP_INCOMPAT.dupttype.d	\
	${TESTSRCDIR}/tst/common/vars/tst.gid.d				\
	${TESTSRCDIR}/tst/common/vars/tst.uid.d				\

# --------------------------------------------------------------------------------
# Tests that currently hang the system:

NOTWORK+= \
	${TESTSRCDIR}/tst/common/fbtprovider/tst.basic.d	\
	${TESTSRCDIR}/tst/common/fbtprovider/tst.return.d	\
	${TESTSRCDIR}/tst/common/grammar/tst.stmts.d		\
	${TESTSRCDIR}/tst/common/misc/tst.enablerace.ksh	\
	${TESTSRCDIR}/tst/common/predicates/tst.predcache.ksh	\
	${TESTSRCDIR}/tst/common/proc/tst.exitkilled.ksh	\

.if ${MACHINE_ARCH} == "i386"

NOTWORK+= ${TESTSRCDIR}/tst/common/dtraceUtil/tst.DataModel64.d.ksh

NOTWORK+= ${TESTSRCDIR}/tst/common/dtraceUtil/tst.DataModel64.d.ksh

# --------------------------------------------------------------------------------
# This test causes i386 to go kaboom in a big way. There is a problem in the
# fbt exception code which needs to be fixed.

NOTWORK+= \
	${TESTSRCDIR}/tst/common/fbtprovider/tst.tailcall.d

# Some tests that don't complete on i386.... :-/

NOTWORK+= ${TESTSRCDIR}/tst/common/funcs/tst.chill.ksh
NOTWORK+= ${TESTSRCDIR}/tst/common/proc/tst.create.ksh
NOTWORK+= ${TESTSRCDIR}/tst/common/proc/tst.exec.ksh
NOTWORK+= ${TESTSRCDIR}/tst/common/proc/tst.execfail.ENOENT.ksh
NOTWORK+= ${TESTSRCDIR}/tst/common/proc/tst.execfail.ksh
NOTWORK+= ${TESTSRCDIR}/tst/common/proc/tst.exitcore.ksh
NOTWORK+= ${TESTSRCDIR}/tst/common/proc/tst.exitexit.ksh
NOTWORK+= ${TESTSRCDIR}/tst/common/proc/tst.exitkilled.ksh
NOTWORK+= ${TESTSRCDIR}/tst/common/proc/tst.signal.ksh
NOTWORK+= ${TESTSRCDIR}/tst/common/proc/tst.sigwait.c
NOTWORK+= ${TESTSRCDIR}/tst/common/proc/tst.sigwait.d
NOTWORK+= ${TESTSRCDIR}/tst/common/proc/tst.startexit.ksh
NOTWORK+= ${TESTSRCDIR}/tst/common/struct/tst.StructDataTypes.d
NOTWORK+= ${TESTSRCDIR}/tst/common/union/tst.UnionDataTypes.d

.endif

# --------------------------------------------------------------------------------
# Interrupt priority isn't relevant on FreeBSD.

NOTWORK+= ${TESTSRCDIR}/tst/common/builtinvar/tst.ipl.d \
	${TESTSRCDIR}/tst/common/builtinvar/tst.ipl1.d

# --------------------------------------------------------------------------------
# These tests relies on the Solaris ping behaviour of exiting after a number of
# pings. On FreeBSD ping doesn't do that.

NOTWORK+= ${TESTSRCDIR}/tst/common/funcs/tst.system.d 		\
	${TESTSRCDIR}/tst/common/ip/tst.ipv6remoteicmp.ksh 	\
	${TESTSRCDIR}/tst/common/ip/tst.ipv6localicmp.ksh 	\
	${TESTSRCDIR}/tst/common/ip/tst.ipv4localicmp.ksh 	\
	${TESTSRCDIR}/tst/common/ip/tst.ipv4remoteicmp.ksh 

# --------------------------------------------------------------------------------
# This test checks for a leading tab on a line before #define. That is illegal
# on Solaris, but the gcc pre-processor on FreeBSD is happy with code like that.

NOTWORK+= ${TESTSRCDIR}/tst/common/preprocessor/err.D_PRAGCTL_INVAL.tabdefine.d

# --------------------------------------------------------------------------------
# This test is coded to use Solaris syscall names. FreeBSD has none with 'lwp"
# in them, so the test fails because of that.

NOTWORK+= ${TESTSRCDIR}/tst/common/probes/tst.probestar.d

NOTYET=	${IGNORE} ${NOTWORK}

# --------------------------------------------------------------------------------
# Only prepare the test lists if we need to:

.if !make(obj) && !make(clean) && !make(cleandir) && !make(listerrors)

# --------------------------------------------------------------------------------
# Get a list of tests, named according to the directory they are in:

_COMMONDIRS!=	cd ${TESTSRCDIR}/tst/common; find . -depth 1 -type d
COMMONDIRS=	${_COMMONDIRS:S/.\///}

listtests	:
	@echo ${COMMONDIRS}

# --------------------------------------------------------------------------------
# Create a test target for each directory:

ALL_TESTFILES=

# Process each common test directory:
.for _d in ${COMMONDIRS}

# Get a list of test files in the test directory:
${_d}_ERR!=	find ${TESTSRCDIR}/tst/common/${_d} -name "err.*.d"
${_d}_TSTD!=	find ${TESTSRCDIR}/tst/common/${_d} -name "tst.*.d"
${_d}_TSTC!=	find ${TESTSRCDIR}/tst/common/${_d} -name "tst.*.c"
${_d}_SHERR!=	find ${TESTSRCDIR}/tst/common/${_d} -name "err.*.ksh"
${_d}_SHTST!=	find ${TESTSRCDIR}/tst/common/${_d} -name "tst.*.ksh"
${_d}_EXE!=	find ${TESTSRCDIR}/tst/common/${_d} -name "tst.*.exe" -o \
						    -name "err.*.exe"
${_d}_TF=	${${_d}_ERR} ${${_d}_TSTD} ${${_d}_SHERR} ${${_d}_SHTST}

# Go through the list of test files and prepare another list that contains
# none of the test files listed in the NOTYET list:
.for _f in ${${_d}_TF}
.if (${NOTYET:M${_f}} == "")
${_d}_TFS+= ${_f}
.endif
.endfor

# Add the list of tests to the overall test list:
ALL_TESTFILES+=	${${_d}_TFS}

.for _f in ${${_d}_TSTC}
.if (${NOTYET:M${_f}} == "")
${_d}_TFCS+= ${_f}
.endif
.endfor

ALL_TESTCFILES+= ${${_d}_TFCS}
ALL_TESTEXEFILES+= ${${_d}_EXE}

${_d}	: ${${_d}_TFCS:T:S/c$/exe/} ${${_d}_EXE:T}
	@${DTEST} ${${_d}_TFS}
.endfor

# --------------------------------------------------------------------------------
# 

PRGS=	${ALL_TESTCFILES:T:S/c$/exe/} ${ALL_TESTEXEFILES:T}

listprgs	:
	@echo ${PRGS}

# --------------------------------------------------------------------------------
# 

.for _f in ${ALL_TESTCFILES}
${_f:T:S/c$/exe/} : ${_f}
	gcc -o ${.TARGET} ${_f} -lrt
.endfor

.for _f in ${ALL_TESTEXEFILES}
${_f:T} : ${_f}
	cp -f ${_f} ${.TARGET}
	chmod a+x ${.TARGET}
.endfor

listcfiles	:
	@echo ${ALL_TESTCFILES}

# --------------------------------------------------------------------------------
# A target to build all tests:

all	: ${PRGS}
	@sync; sync; sync; sync; sync
	@${DTEST} ${ALL_TESTFILES}

# --------------------------------------------------------------------------------
# A target to build the tests flagged as not working yet:

ignore	:
	@${DTEST} ${IGNORE}

.endif

listerrors	:
	@find ${.OBJDIR} -name "*.err" -exec cat {} \; -print


# --------------------------------------------------------------------------------
# Test failures are written to failure.N directories in the object tree.
# Get a list of directories in the object tree for 'make clean'.

.if ${.OBJDIR} != ${.CURDIR} && !empty(${.OBJDIR})
CLEANDIRS!=	cd ${.OBJDIR} && ls -d *
.endif

# --------------------------------------------------------------------------------
# Insist on using an object directory. Things get messy otherwsie.

.if ${.OBJDIR} == ${.CURDIR} && !make(obj) && !make(clean) && !make(cleandir)
.error ERROR: You must run 'make obj' first!
.endif

# --------------------------------------------------------------------------------
# Load the DTrace module set if necessary.

.BEGIN	:
	@if ! kldstat -q -m dtrace_test; then kldload dtrace_test; fi

.include <bsd.obj.mk>

