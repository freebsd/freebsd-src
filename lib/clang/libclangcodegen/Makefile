# $FreeBSD$

.include <src.opts.mk>

LIB=	clangcodegen

SRCDIR=	tools/clang/lib/CodeGen
SRCS=	BackendUtil.cpp \
	CGAtomic.cpp \
	CGBlocks.cpp \
	CGBuiltin.cpp \
	CGCUDABuiltin.cpp \
	CGCUDANV.cpp \
	CGCUDARuntime.cpp \
	CGCXX.cpp \
	CGCXXABI.cpp \
	CGCall.cpp \
	CGClass.cpp \
	CGCleanup.cpp \
	CGDebugInfo.cpp \
	CGDecl.cpp \
	CGDeclCXX.cpp \
	CGException.cpp \
	CGExpr.cpp \
	CGExprAgg.cpp \
	CGExprCXX.cpp \
	CGExprComplex.cpp \
	CGExprConstant.cpp \
	CGExprScalar.cpp \
	CGLoopInfo.cpp \
	CGObjC.cpp \
	CGObjCGNU.cpp \
	CGObjCMac.cpp \
	CGObjCRuntime.cpp \
	CGOpenCLRuntime.cpp \
	CGOpenMPRuntime.cpp \
	CGOpenMPRuntimeNVPTX.cpp \
	CGRecordLayoutBuilder.cpp \
	CGStmt.cpp \
	CGStmtOpenMP.cpp \
	CGVTT.cpp \
	CGVTables.cpp \
	CodeGenAction.cpp \
	CodeGenFunction.cpp \
	CodeGenModule.cpp \
	CodeGenPGO.cpp \
	CodeGenTBAA.cpp \
	CodeGenTypes.cpp \
	CoverageMappingGen.cpp \
	ItaniumCXXABI.cpp \
	MicrosoftCXXABI.cpp \
	ModuleBuilder.cpp \
	ObjectFilePCHContainerOperations.cpp \
	SanitizerMetadata.cpp \
	SwiftCallingConv.cpp \
	TargetInfo.cpp

.if ${MK_CLANG_EXTRAS} != "no"
SRCS+=	CodeGenABITypes.cpp
.endif

TGHDRS=	AttrList \
	AttrParsedAttrList \
	AttrVisitor \
	Attributes \
	Attrs \
	CommentCommandList \
	CommentNodes \
	DeclNodes \
	DiagnosticCommonKinds \
	DiagnosticFrontendKinds \
	DiagnosticSemaKinds \
	Intrinsics \
	StmtNodes \
	arm_neon

.include "../clang.lib.mk"
