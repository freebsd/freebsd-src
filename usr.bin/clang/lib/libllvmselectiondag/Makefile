# $FreeBSD$

LIB=	llvmselectiondag

SRCDIR=	lib/CodeGen/SelectionDAG
SRCS=	CallingConvLower.cpp DAGCombiner.cpp FastISel.cpp \
	LegalizeDAG.cpp LegalizeFloatTypes.cpp \
	LegalizeIntegerTypes.cpp LegalizeTypes.cpp \
	LegalizeTypesGeneric.cpp LegalizeVectorOps.cpp \
	LegalizeVectorTypes.cpp ScheduleDAGFast.cpp \
	ScheduleDAGList.cpp ScheduleDAGRRList.cpp \
	ScheduleDAGSDNodes.cpp ScheduleDAGSDNodesEmit.cpp \
	SelectionDAG.cpp SelectionDAGBuild.cpp SelectionDAGISel.cpp \
	SelectionDAGPrinter.cpp TargetLowering.cpp

TGHDRS=	Intrinsics

.include "../../clang.lib.mk"
