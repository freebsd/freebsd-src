#ifndef LLVM_TARGET_IA64_TARGETLOWERING_H
#define LLVM_TARGET_IA64_TARGETLOWERING_H

#include "IA64.h"
#include "llvm/CodeGen/SelectionDAG.h"
#include "llvm/Target/TargetLowering.h"

namespace llvm {

  class IA64Subtarget;
  class IA64TargetMachine;

  class IA64TargetLowering : public TargetLowering {
    const TargetData *TD;
    const IA64Subtarget &Subtarget;
    const IA64TargetMachine &TM;

  public:
    explicit IA64TargetLowering(IA64TargetMachine &TM);

    virtual SDValue LowerFormalArguments(SDValue Chain,
        CallingConv::ID CallConv, bool isVarArg,
        const SmallVectorImpl<ISD::InputArg> &Ins, DebugLoc dl,
        SelectionDAG &DAG, SmallVectorImpl<SDValue> &InVals) const;

    virtual SDValue LowerReturn(SDValue Chain, CallingConv::ID CallConv,
        bool isVarArg, const SmallVectorImpl<ISD::OutputArg> &Outs,
        const SmallVectorImpl<SDValue> &OutVals, DebugLoc dl,
        SelectionDAG &DAG) const;
  };

} // namespace llvm

#endif // LLVM_TARGET_IA64_TARGETLOWERING_H
