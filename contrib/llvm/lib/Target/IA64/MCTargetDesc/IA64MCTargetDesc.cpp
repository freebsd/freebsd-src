#include "IA64MCTargetDesc.h"
#include "IA64MCAsmInfo.h"
#include "InstPrinter/IA64InstPrinter.h"
#include "llvm/MC/MCCodeGenInfo.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/Support/TargetRegistry.h"

#define GET_INSTRINFO_MC_DESC
#include "IA64GenInstrInfo.inc"

#define GET_SUBTARGETINFO_MC_DESC
#include "IA64GenSubtargetInfo.inc"

#define GET_REGINFO_MC_DESC
#include "IA64GenRegisterInfo.inc"

using namespace llvm;

static MCInstrInfo *
createIA64MCInstrInfo()
{
  MCInstrInfo *X = new MCInstrInfo();
  InitIA64MCInstrInfo(X);
  return X;
}

static MCRegisterInfo *
createIA64MCRegisterInfo(StringRef TT)
{
  MCRegisterInfo *X = new MCRegisterInfo();
  InitIA64MCRegisterInfo(X, IA64::B0);
  return X;
}

static MCSubtargetInfo *
createIA64MCSubtargetInfo(StringRef TT, StringRef CPU, StringRef FS)
{
  MCSubtargetInfo *X = new MCSubtargetInfo();
  InitIA64MCSubtargetInfo(X, TT, CPU, FS);
  return X;
}

static MCCodeGenInfo *
createIA64MCCodeGenInfo(StringRef TT, Reloc::Model RM, CodeModel::Model CM)
{
  MCCodeGenInfo *X = new MCCodeGenInfo();
  X->InitMCCodeGenInfo(RM, CM);
  return X;
}

static MCInstPrinter *
createIA64MCInstPrinter(const Target &T, unsigned SyntaxVariant,
      const MCAsmInfo &MAI, const MCSubtargetInfo &STI)
{
  if (SyntaxVariant == 0)
    return new IA64InstPrinter(MAI);
  return 0;
}

extern "C" void
LLVMInitializeIA64TargetMC()
{
  // Register the MC asm info.
  RegisterMCAsmInfo<IA64MCAsmInfo> X(TheIA64Target);

  // Register the MC codegen info.
  TargetRegistry::RegisterMCCodeGenInfo(TheIA64Target, createIA64MCCodeGenInfo);

  // Register the MC instruction info.
  TargetRegistry::RegisterMCInstrInfo(TheIA64Target, createIA64MCInstrInfo);

  // Register the MC register info.
  TargetRegistry::RegisterMCRegInfo(TheIA64Target, createIA64MCRegisterInfo);

  // Register the MC subtarget info.
  TargetRegistry::RegisterMCSubtargetInfo(TheIA64Target,
        createIA64MCSubtargetInfo);

  // Register the MCInstPrinter.
  TargetRegistry::RegisterMCInstPrinter(TheIA64Target, createIA64MCInstPrinter);
}
