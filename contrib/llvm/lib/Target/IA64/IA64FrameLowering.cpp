#define DEBUG_TYPE "ia64-frame-lowering"

#include "IA64FrameLowering.h"
#include "IA64InstrInfo.h"
#include "IA64MachineFunctionInfo.h"

#include "llvm/Function.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/Target/TargetData.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"

using namespace llvm;

void
IA64FrameLowering::emitPrologue(MachineFunction &MF) const
{
  IA64MachineFunctionInfo *IA64FI = MF.getInfo<IA64MachineFunctionInfo>();
  MachineFrameInfo *MFI = MF.getFrameInfo();
  MachineBasicBlock &MBB = MF.front();
  const IA64InstrInfo &TII =
        *static_cast<const IA64InstrInfo*>(MF.getTarget().getInstrInfo());

  DEBUG(dbgs() << "XXX: IA64FrameLowering::" << __func__ << "\n");
}

void
IA64FrameLowering::emitEpilogue(MachineFunction &MF, MachineBasicBlock &MBB)
    const
{

  DEBUG(dbgs() << "XXX: IA64FrameLowering::" << __func__ << "\n");
}

bool
IA64FrameLowering::hasFP(const MachineFunction &MF) const
{ 
  llvm_unreachable(__func__);
}
