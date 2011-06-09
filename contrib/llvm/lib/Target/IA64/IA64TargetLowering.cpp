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

IA64TargetLowering::IA64TargetLowering(IA64TargetMachine &tm) :
    TargetLowering(tm, new TargetLoweringObjectFileELF()),
    Subtarget(*tm.getSubtargetImpl()),
    TM(tm)
{
  TD = getTargetData();

  // Set up the register classes.
  addRegisterClass(MVT::f128, &IA64::FloatingPointRegClass);
  addRegisterClass(MVT::i64, &IA64::GeneralRegClass);
  addRegisterClass(MVT::i1, &IA64::PredicateRegClass);

  // Compute derived properties from the register classes
  computeRegisterProperties();
}

unsigned
IA64TargetLowering::getFunctionAlignment(const Function *F) const
{
  // Functions must have at least 16-byte alignment, but 32-byte alignment
  // is better because branch targets should be aligned on 32-byte boundaries
  // to ensure that the front-end can deliver 2 bundles per cycle to the
  // back-end.
  return F->hasFnAttr(Attribute::OptimizeForSize) ? 4 : 5;
}
