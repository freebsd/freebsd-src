# $FreeBSD$

.include <src.opts.mk>

LIB=	llvmsupport

SRCDIR=	lib/Support
SRCS=	APFloat.cpp \
	APInt.cpp \
	APSInt.cpp \
	ARMBuildAttrs.cpp \
	Allocator.cpp \
	Atomic.cpp \
	BlockFrequency.cpp \
	BranchProbability.cpp \
	CachePruning.cpp \
	CommandLine.cpp \
	Compression.cpp \
	ConvertUTF.c \
	ConvertUTFWrapper.cpp \
	CrashRecoveryContext.cpp \
	DAGDeltaAlgorithm.cpp \
	DataExtractor.cpp \
	Debug.cpp \
	DeltaAlgorithm.cpp \
	Dwarf.cpp \
	DynamicLibrary.cpp \
	Errno.cpp \
	Error.cpp \
	ErrorHandling.cpp \
	FoldingSet.cpp \
	FormattedStream.cpp \
	GraphWriter.cpp \
	Hashing.cpp \
	Host.cpp \
	IntEqClasses.cpp \
	IntervalMap.cpp \
	IntrusiveRefCntPtr.cpp \
	JamCRC.cpp \
	LEB128.cpp \
	LineIterator.cpp \
	Locale.cpp \
	LockFileManager.cpp \
	MD5.cpp \
	ManagedStatic.cpp \
	MathExtras.cpp \
	MemoryBuffer.cpp \
	MemoryObject.cpp \
	Mutex.cpp \
	Options.cpp \
	Path.cpp \
	PluginLoader.cpp \
	PrettyStackTrace.cpp \
	Process.cpp \
	Program.cpp \
	RWMutex.cpp \
	RandomNumberGenerator.cpp \
	Regex.cpp \
	SHA1.cpp \
	ScaledNumber.cpp \
	ScopedPrinter.cpp \
	SearchForAddressOfSpecialSymbol.cpp \
	Signals.cpp \
	SmallPtrSet.cpp \
	SmallVector.cpp \
	SourceMgr.cpp \
	SpecialCaseList.cpp \
	Statistic.cpp \
	StreamingMemoryObject.cpp \
	StringExtras.cpp \
	StringMap.cpp \
	StringRef.cpp \
	StringSaver.cpp \
	TargetParser.cpp \
	TargetRegistry.cpp \
	ThreadLocal.cpp \
	Threading.cpp \
	TimeValue.cpp \
	Timer.cpp \
	ToolOutputFile.cpp \
	Triple.cpp \
	Twine.cpp \
	Unicode.cpp \
	Valgrind.cpp \
	YAMLParser.cpp \
	YAMLTraits.cpp \
	circular_raw_ostream.cpp \
	raw_os_ostream.cpp \
	raw_ostream.cpp \
	regcomp.c \
	regerror.c \
	regexec.c \
	regfree.c \
	regstrlcpy.c

.if ${MK_CLANG_EXTRAS} != "no"
SRCS+=	ARMWinEH.cpp \
	COM.cpp \
	DataStream.cpp \
	FileOutputBuffer.cpp \
	FileUtilities.cpp \
	SystemUtils.cpp \
	ThreadPool.cpp \
	Watchdog.cpp
.endif

.if ${MK_CLANG_EXTRAS} != "no" || ${MK_LLDB} != "no"
SRCS+=	Memory.cpp
.endif

.if ${MK_LLDB} != "no"
SRCS+=	StringPool.cpp
.endif

.include "../clang.lib.mk"

# Ugly hack to work around CLOCK_PROCESS_CPUTIME_ID not being properly defined
# between r239347 and r245428.
CXXFLAGS.Process.cpp=	-DCLOCK_PROCESS_CPUTIME_ID=15
