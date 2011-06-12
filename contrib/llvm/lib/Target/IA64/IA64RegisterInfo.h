#ifndef LLVM_TARGET_IA64_REGISTERINFO_H
#define LLVM_TARGET_IA64_REGISTERINFO_H

#include "llvm/Target/TargetRegisterInfo.h"

#include "IA64GenRegisterInfo.h.inc"

namespace llvm {

  class TargetInstrInfo;
  class IA64TargetMachine;

  struct IA64RegisterInfo : public IA64GenRegisterInfo {
  private:
    IA64TargetMachine &TM;
    const TargetInstrInfo &TII;

    /// StackAlign - Default stack alignment.
    ///
    unsigned StackAlign;

  public:
    IA64RegisterInfo(IA64TargetMachine &tm, const TargetInstrInfo &tii);

    const unsigned *getCalleeSavedRegs(const MachineFunction *MF = 0) const;

    BitVector getReservedRegs(const MachineFunction &MF) const;

    void eliminateFrameIndex(MachineBasicBlock::iterator II, int SPAdj,
        RegScavenger *RS = NULL) const;

    int getDwarfRegNum(unsigned RegNum, bool isEH) const;
    int getLLVMRegNum(unsigned RegNum, bool isEH) const;

    unsigned getFrameRegister(const MachineFunction &MF) const;
    unsigned getRARegister() const;
  };

} // namespace llvm

#endif // LLVM_TARGET_IA64_REGISTERINFO_H
