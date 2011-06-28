#ifndef LLVM_TARGET_IA64_FRAMELOWERING_H
#define LLVM_TARGET_IA64_FRAMELOWERING_H

#include "IA64.h"
#include "IA64Subtarget.h"
#include "llvm/Target/TargetFrameLowering.h"

namespace llvm {

  class IA64Subtarget;

  class IA64FrameLowering : public TargetFrameLowering {
  protected:
    const IA64Subtarget &STI;

  public:
    explicit IA64FrameLowering(const IA64Subtarget &sti) :
	TargetFrameLowering(TargetFrameLowering::StackGrowsDown, 16, 0, 16),
	STI(sti) {}

    void emitPrologue(MachineFunction &MF) const;
    void emitEpilogue(MachineFunction &MF, MachineBasicBlock &MBB) const;

    bool hasFP(const MachineFunction &MF) const;
  };

} // namespace llvm

#endif // LLVM_TARGET_IA64_FRAMELOWERING_H
