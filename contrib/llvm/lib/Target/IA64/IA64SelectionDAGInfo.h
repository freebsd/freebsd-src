#ifndef LLVM_TARGET_IA64_SELECTIONDAGINFO_H
#define LLVM_TARGET_IA64_SELECTIONDAGINFO_H

#include "llvm/Target/TargetSelectionDAGInfo.h"

namespace llvm {

  class IA64TargetMachine;

  class IA64SelectionDAGInfo : public TargetSelectionDAGInfo {
  public:
    explicit IA64SelectionDAGInfo(const IA64TargetMachine &TM);
    ~IA64SelectionDAGInfo();
};

} // namespace llvm

#endif // LLVM_TARGET_IA64_SELECTIONDAGINFO_H
