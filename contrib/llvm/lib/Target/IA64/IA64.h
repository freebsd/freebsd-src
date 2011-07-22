#ifndef LLVM_TARGET_IA64_H
#define LLVM_TARGET_IA64_H

#include "MCTargetDesc/IA64MCTargetDesc.h"

#include "llvm/Target/TargetMachine.h"

namespace llvm {

  class FunctionPass;
  class IA64TargetMachine;

  FunctionPass *createIA64ISelPass(IA64TargetMachine &TM,
        CodeGenOpt::Level OptLevel);

  FunctionPass *createIA64BundleSelectionPass();

} // namespace llvm

#endif // LLVM_TARGET_IA64_H
