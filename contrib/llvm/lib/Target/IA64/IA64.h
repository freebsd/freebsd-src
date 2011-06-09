#ifndef LLVM_TARGET_IA64_H
#define LLVM_TARGET_IA64_H

#include "llvm/Target/TargetMachine.h"

namespace llvm {

  class FunctionPass;
  class IA64TargetMachine;

  FunctionPass *createIA64ISelDag(IA64TargetMachine &TM,
	CodeGenOpt::Level OptLevel);

  FunctionPass *createIA64BundleSelectionPass();

  extern Target TheIA64Target;

} // namespace llvm

#include "IA64GenRegisterNames.inc"
#include "IA64GenInstrNames.inc"

#endif // LLVM_TARGET_IA64_H
