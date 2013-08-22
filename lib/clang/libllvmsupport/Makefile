# $FreeBSD$

.include <bsd.own.mk>

LIB=	llvmsupport

SRCDIR=	lib/Support
SRCS=	APFloat.cpp \
	APInt.cpp \
	APSInt.cpp \
	Allocator.cpp \
	Atomic.cpp \
	BlockFrequency.cpp \
	BranchProbability.cpp \
	CommandLine.cpp \
	ConstantRange.cpp \
	ConvertUTF.c \
	ConvertUTFWrapper.cpp \
	CrashRecoveryContext.cpp \
	DAGDeltaAlgorithm.cpp \
	Debug.cpp \
	DeltaAlgorithm.cpp \
	Dwarf.cpp \
	DynamicLibrary.cpp \
	Errno.cpp \
	ErrorHandling.cpp \
	FileOutputBuffer.cpp \
	FoldingSet.cpp \
	FormattedStream.cpp \
	GraphWriter.cpp \
	Hashing.cpp \
	Host.cpp \
	IncludeFile.cpp \
	IntEqClasses.cpp \
	IntervalMap.cpp \
	IntrusiveRefCntPtr.cpp \
	IsInf.cpp \
	IsNAN.cpp \
	Locale.cpp \
	LockFileManager.cpp \
	ManagedStatic.cpp \
	Memory.cpp \
	MemoryBuffer.cpp \
	MemoryObject.cpp \
	Mutex.cpp \
	Path.cpp \
	PathV2.cpp \
	PluginLoader.cpp \
	PrettyStackTrace.cpp \
	Process.cpp \
	Program.cpp \
	RWMutex.cpp \
	Regex.cpp \
	SearchForAddressOfSpecialSymbol.cpp \
	Signals.cpp \
	SmallPtrSet.cpp \
	SmallVector.cpp \
	SourceMgr.cpp \
	Statistic.cpp \
	StreamableMemoryObject.cpp \
	StringExtras.cpp \
	StringMap.cpp \
	StringPool.cpp \
	StringRef.cpp \
	TargetRegistry.cpp \
	ThreadLocal.cpp \
	Threading.cpp \
	TimeValue.cpp \
	Timer.cpp \
	ToolOutputFile.cpp \
	Triple.cpp \
	Twine.cpp \
	Valgrind.cpp \
	Watchdog.cpp \
	YAMLParser.cpp \
	YAMLTraits.cpp \
	circular_raw_ostream.cpp \
	raw_os_ostream.cpp \
	raw_ostream.cpp \
	regcomp.c \
	regerror.c \
	regexec.c \
	regfree.c \
	regstrlcpy.c \
	system_error.cpp

.if ${MK_CLANG_EXTRAS} != "no"
SRCS+=	DataExtractor.cpp \
	DataStream.cpp \
	Disassembler.cpp \
	FileUtilities.cpp \
	SystemUtils.cpp
.endif

.include "../clang.lib.mk"
