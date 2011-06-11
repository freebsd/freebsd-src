#ifndef LLVM_TARGET_IA64_MACHINEFUNCTIONINFO_H
#define LLVM_TARGET_IA64_MACHINEFUNCTIONINFO_H

#include "llvm/CodeGen/MachineFunction.h"

namespace llvm {

  class IA64MachineFunctionInfo : public MachineFunctionInfo {
    // Nothing yet.
  public:
    IA64MachineFunctionInfo() {}

    explicit IA64MachineFunctionInfo(MachineFunction &MF) {}
  };

} // namespace llvm

#endif // LLVM_TARGET_IA64_MACHINEFUNCTIONINFO_H
