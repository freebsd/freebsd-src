#ifndef LLVM_TARGET_IA64_INSTRINFO_H
#define LLVM_TARGET_IA64_INSTRINFO_H

#include "IA64RegisterInfo.h"

#include "llvm/Target/TargetInstrInfo.h"

namespace llvm {

  class IA64TargetMachine;

  class IA64InstrInfo : public TargetInstrInfoImpl {
    const IA64RegisterInfo RI;
    IA64TargetMachine &TM;

  public:
    explicit IA64InstrInfo(IA64TargetMachine &TM);

    /// getRegisterInfo - TargetInstrInfo is a superset of MRegister info.  As
    /// such, whenever a client has an instance of instruction info, it should
    /// always be able to get register info as well (through this method).
    ///
    virtual const TargetRegisterInfo &getRegisterInfo() const { return RI; }

    virtual void copyPhysReg(MachineBasicBlock &MBB,
        MachineBasicBlock::iterator MI, DebugLoc DL, unsigned DestReg,
        unsigned SrcReg, bool KillSrc) const;
  };

} // namespace llvm

#endif // LLVM_TARGET_IA64_INSTRINFO_H
