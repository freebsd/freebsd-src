#include "IA64.h"
#include "IA64TargetMachine.h"

#include "llvm/PassManager.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/Support/TargetRegistry.h"

using namespace llvm;

extern "C" void
LLVMInitializeIA64Target() {
  RegisterTargetMachine<IA64TargetMachine> X(TheIA64Target);
}

IA64TargetMachine::IA64TargetMachine(const Target &T, StringRef TT,
        StringRef CPU, StringRef FS, Reloc::Model RM, CodeModel::Model CM) :
    LLVMTargetMachine(T, TT, CPU, FS, RM, CM),
    Subtarget(TT, CPU, FS),
    DataLayout("e-i64:64:64-f80:128:128-f128:128:128-n8:16:32:64"),
    InstrInfo(*this),
    TLInfo(*this),
    TSInfo(*this),
    FrameLowering(Subtarget)
{
  // Nothing to do.
}

bool
IA64TargetMachine::addInstSelector(PassManagerBase &PM,
	CodeGenOpt::Level OptLevel)
{
  PM.add(createIA64ISelPass(*this, OptLevel));
  return false;
}

bool
IA64TargetMachine::addPreEmitPass(PassManagerBase &PM,
	CodeGenOpt::Level OptLevel)
{
  // PM.add(createIA64BundleSelectionPass());
  return false;
}
