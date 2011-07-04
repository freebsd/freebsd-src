#define DEBUG_TYPE "ia64-target-lowering"

#include "IA64.h"
#include "IA64Subtarget.h"
#include "IA64TargetLowering.h"
#include "IA64TargetMachine.h"

#include "llvm/DerivedTypes.h"
#include "llvm/Function.h"
#include "llvm/Intrinsics.h"
#include "llvm/CallingConv.h"
#include "llvm/GlobalVariable.h"
#include "llvm/GlobalAlias.h"
#include "llvm/CodeGen/CallingConvLower.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/PseudoSourceValue.h"
#include "llvm/CodeGen/SelectionDAGISel.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"
#include "llvm/CodeGen/ValueTypes.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/VectorExtras.h"

using namespace llvm;

#include "IA64GenCallingConv.inc"

IA64TargetLowering::IA64TargetLowering(IA64TargetMachine &tm) :
    TargetLowering(tm, new TargetLoweringObjectFileELF()),
    Subtarget(*tm.getSubtargetImpl()),
    TM(tm)
{
  TD = getTargetData();

  // Set up the register classes.
  addRegisterClass(MVT::i64, &IA64::BRRegClass);
  addRegisterClass(MVT::f128, &IA64::FRRegClass);
  addRegisterClass(MVT::i64, &IA64::GRRegClass);
  addRegisterClass(MVT::i1, &IA64::PRegClass);

  // Compute derived properties from the register classes
  computeRegisterProperties();

  setMinFunctionAlignment(4);
  setPrefFunctionAlignment(5);

  setJumpBufSize(512);
  setJumpBufAlignment(16);
}

const char *
IA64TargetLowering::getTargetNodeName(unsigned Opcode) const
{
  const char *nn;

  switch (Opcode) {
  case IA64ISD::RET_FLAG:	nn = "IA64ISD::RET_FLAG"; break;
  default:			nn = NULL; break;
  }
  return nn;
}

SDValue
IA64TargetLowering::LowerCall(SDValue Chain, SDValue Callee,
    CallingConv::ID CallConv, bool isVarArg, bool &isTailCall,
    const SmallVectorImpl<ISD::OutputArg> &Outs,
    const SmallVectorImpl<SDValue> &OutVals,
    const SmallVectorImpl<ISD::InputArg> &Ins, DebugLoc dl, SelectionDAG &DAG,
    SmallVectorImpl<SDValue> &InVals) const
{

  DEBUG(dbgs() << "XXX: IA64TargetLowering::" <<__func__ << "\n");

  return Chain;
}

SDValue
IA64TargetLowering::LowerFormalArguments(SDValue Chain,
    CallingConv::ID CallConv, bool isVarArg,
    const SmallVectorImpl<ISD::InputArg> &Ins, DebugLoc dl, SelectionDAG &DAG,
    SmallVectorImpl<SDValue> &InVals) const
{
  MachineFunction &MF = DAG.getMachineFunction();
  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CallConv, isVarArg, MF, getTargetMachine(), ArgLocs,
      *DAG.getContext());

  DEBUG(dbgs() << "XXX: IA64TargetLowering::" << __func__ << "\n");

  CCInfo.AllocateStack(0, 8);
  CCInfo.AnalyzeFormalArguments(Ins, CC_IA64);

  for (unsigned i = 0, e = ArgLocs.size(); i != e; ++i) {
    CCValAssign &VA = ArgLocs[i];
    EVT ValVT = VA.getValVT();
    TargetRegisterClass *RC;

    if (!VA.isRegLoc())
      llvm_unreachable(__func__);

    switch (ValVT.getSimpleVT().SimpleTy) {
    case MVT::i64:
      RC = &IA64::GRRegClass;
      break;
    default:
      llvm_unreachable(__func__);
    }

    unsigned Reg = MF.addLiveIn(VA.getLocReg(), RC);
    SDValue ArgValue = DAG.getCopyFromReg(Chain, dl, Reg, ValVT);
    InVals.push_back(ArgValue);

    DEBUG(dbgs() << i << ": " << ValVT.getSimpleVT().SimpleTy << " -> " <<
          Reg << "\n");
  }

  return Chain;
}

SDValue
IA64TargetLowering::LowerReturn(SDValue Chain, CallingConv::ID CallConv,
    bool isVarArg, const SmallVectorImpl<ISD::OutputArg> &Outs,
    const SmallVectorImpl<SDValue> &OutVals, DebugLoc dl,
    SelectionDAG &DAG) const
{
  MachineFunction &MF = DAG.getMachineFunction();
  SmallVector<CCValAssign, 16> RVLocs;
  CCState CCInfo(CallConv, isVarArg, MF, getTargetMachine(), RVLocs,
      *DAG.getContext());
  CCInfo.AnalyzeReturn(Outs, RetCC_IA64);

  DEBUG(dbgs() << "XXX: IA64TargetLowering::" <<__func__ << "\n");

  if (MF.getRegInfo().liveout_empty()) {
    for (unsigned i = 0; i != RVLocs.size(); ++i)
      MF.getRegInfo().addLiveOut(RVLocs[i].getLocReg());
  }

  SDValue Flag;

  for (unsigned i = 0; i != RVLocs.size(); ++i) {
    CCValAssign &VA = RVLocs[i];

    if (!VA.isRegLoc())
      llvm_unreachable(__func__);

    Chain = DAG.getCopyToReg(Chain, dl, VA.getLocReg(), OutVals[i], Flag);
    Flag = Chain.getValue(1);
  }

  SDValue result;
  if (Flag.getNode())
    result = DAG.getNode(IA64ISD::RET_FLAG, dl, MVT::Other, Chain, Flag);
  else
    result = DAG.getNode(IA64ISD::RET_FLAG, dl, MVT::Other, Chain);
  return result;
}
