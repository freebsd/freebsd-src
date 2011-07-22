#include "IA64MCTargetDesc.h"
#include "IA64MCAsmInfo.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/Target/TargetRegistry.h"

#define GET_INSTRINFO_MC_DESC
#include "IA64GenInstrInfo.inc"

#define GET_SUBTARGETINFO_MC_DESC
#include "IA64GenSubtargetInfo.inc"

#define GET_REGINFO_MC_DESC
#include "IA64GenRegisterInfo.inc"

using namespace llvm;

static
MCInstrInfo *createIA64MCInstrInfo()
{
  MCInstrInfo *X = new MCInstrInfo();
  InitIA64MCInstrInfo(X);
  return X;
}

extern "C" void
LLVMInitializeIA64MCInstrInfo()
{
  TargetRegistry::RegisterMCInstrInfo(TheIA64Target, createIA64MCInstrInfo);
}

static
MCSubtargetInfo *createIA64MCSubtargetInfo(StringRef TT, StringRef CPU,
      StringRef FS)
{
  MCSubtargetInfo *X = new MCSubtargetInfo();
  InitIA64MCSubtargetInfo(X, TT, CPU, FS);
  return X;
}

extern "C" void
LLVMInitializeIA64MCSubtargetInfo()
{
  TargetRegistry::RegisterMCSubtargetInfo(TheIA64Target,
      createIA64MCSubtargetInfo);
}

extern "C" void
LLVMInitializeIA64MCAsmInfo()
{
  RegisterMCAsmInfo<IA64MCAsmInfo> X(TheIA64Target);
}
