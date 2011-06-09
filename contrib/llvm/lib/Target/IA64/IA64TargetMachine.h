#ifndef LLVM_TARGET_IA64_TARGETMACHINE_H
#define LLVM_TARGET_IA64_TARGETMACHINE_H

#include "IA64FrameLowering.h"
#include "IA64InstrInfo.h"
#include "IA64SelectionDAGInfo.h"
#include "IA64Subtarget.h"
#include "IA64TargetLowering.h"

#include "llvm/Target/TargetData.h"
#include "llvm/Target/TargetFrameLowering.h"
#include "llvm/Target/TargetMachine.h"

namespace llvm {

  class IA64TargetMachine : public LLVMTargetMachine {
    IA64Subtarget        Subtarget;
    const TargetData     DataLayout;       // Calculates type size & alignment
    IA64InstrInfo        InstrInfo;
    IA64TargetLowering   TLInfo;
    IA64SelectionDAGInfo TSInfo;
    IA64FrameLowering    FrameLowering;

  public:
    IA64TargetMachine(const Target &T, const std::string &TT,
	const std::string &FS);

    virtual const TargetFrameLowering *getFrameLowering() const {
      return &FrameLowering;
    }

    virtual const IA64InstrInfo *getInstrInfo() const { return &InstrInfo; }
    virtual const TargetData *getTargetData() const { return &DataLayout;}

    virtual const IA64Subtarget *getSubtargetImpl() const {
      return &Subtarget;
    }

    virtual const TargetRegisterInfo *getRegisterInfo() const {
      return &InstrInfo.getRegisterInfo();
    }

    virtual const IA64TargetLowering *getTargetLowering() const {
      return &TLInfo;
    }

    virtual const IA64SelectionDAGInfo* getSelectionDAGInfo() const {
      return &TSInfo;
    }

    virtual bool addInstSelector(PassManagerBase &PM,
	CodeGenOpt::Level OptLevel);
    virtual bool addPreEmitPass(PassManagerBase &PM,
	CodeGenOpt::Level OptLevel);
  };

} // namespace llvm

#endif // LLVM_TARGET_IA64_TARGETMACHINE_H
