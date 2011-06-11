#include "IA64.h"
#include "IA64RegisterInfo.h"
#include "IA64TargetMachine.h"

#include "llvm/Function.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

// FIXME: Provide proper call frame setup / destroy opcodes.
IA64RegisterInfo::IA64RegisterInfo(IA64TargetMachine &tm,
	const TargetInstrInfo &tii) :
    IA64GenRegisterInfo(IA64::NOP, IA64::NOP),
    TM(tm),
    TII(tii)
{
  StackAlign = TM.getFrameLowering()->getStackAlignment();
}

const unsigned *
IA64RegisterInfo::getCalleeSavedRegs(const MachineFunction *MF) const
{
  static const unsigned preservedRegisters[] = {
    IA64::R4, IA64::R5, IA64::R6, IA64::R7,
    IA64::F2, IA64::F3, IA64::F4, IA64::F5,
    IA64::F16, IA64::F17, IA64::F18, IA64::F19,
    IA64::F20, IA64::F21, IA64::F22, IA64::F23,
    IA64::F24, IA64::F25, IA64::F26, IA64::F27,
    IA64::F28, IA64::F29, IA64::F30, IA64::F31,
    IA64::P1, IA64::P2, IA64::P3, IA64::P4,
    IA64::P5,
    IA64::P16, IA64::P17, IA64::P18, IA64::P19,
    IA64::P20, IA64::P21, IA64::P22, IA64::P23,
    IA64::P24, IA64::P25, IA64::P26, IA64::P27,
    IA64::P28, IA64::P29, IA64::P30, IA64::P31,
    IA64::P32, IA64::P33, IA64::P34, IA64::P35,
    IA64::P36, IA64::P37, IA64::P38, IA64::P39,
    IA64::P40, IA64::P41, IA64::P42, IA64::P43,
    IA64::P44, IA64::P45, IA64::P46, IA64::P47,
    IA64::P48, IA64::P49, IA64::P50, IA64::P51,
    IA64::P52, IA64::P53, IA64::P54, IA64::P55,
    IA64::P56, IA64::P57, IA64::P58, IA64::P59,
    IA64::P60, IA64::P61, IA64::P62, IA64::P63,
    IA64::B1, IA64::B2, IA64::B3, IA64::B4,
    IA64::B5
  };

  // XXX TODO
  // Predicate registers cannot be saved/restored individually.
  // It is done by saving the 64-bit 'pr' predicate register.
  // It's the superset of all 1-bit predicate registers.

  return preservedRegisters;
}

BitVector
IA64RegisterInfo::getReservedRegs(const MachineFunction &MF) const
{
  BitVector rsvd(getNumRegs());

  rsvd.set(IA64::R0); // constant, hardwired 0
  rsvd.set(IA64::R1); // global data pointer (aka gp)
  rsvd.set(IA64::R12); // (memory) stack pointer (aka sp)
  rsvd.set(IA64::R13); // thread pointer (aka tp)

  rsvd.set(IA64::F0); // constant, hardwired 0.0
  rsvd.set(IA64::F1); // constant, hardwired 1.0

  rsvd.set(IA64::P0); // constant, hardwired 1 (true)

  return rsvd;
}

void
IA64RegisterInfo::eliminateFrameIndex(MachineBasicBlock::iterator MI, int SPAdj,
    RegScavenger *RS) const
{
  llvm_unreachable(__func__);
}

int
IA64RegisterInfo::getDwarfRegNum(unsigned RegNum, bool isEH) const
{
  return IA64GenRegisterInfo::getDwarfRegNumFull(RegNum, 0);
}

unsigned
IA64RegisterInfo::getFrameRegister(const MachineFunction &MF) const
{
  llvm_unreachable(__func__);
}

unsigned
IA64RegisterInfo::getRARegister() const
{
  llvm_unreachable(__func__);
}

#include "IA64GenRegisterInfo.inc"
