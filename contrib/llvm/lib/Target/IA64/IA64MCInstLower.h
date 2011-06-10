#ifndef LLVM_TARGET_IA64_MCINSTLOWER_H
#define LLVM_TARGET_IA64_MCINSTLOWER_H

#include "llvm/Support/Compiler.h"

namespace llvm {

  class AsmPrinter;
  class MCAsmInfo;
  class MCContext;
  class MCInst;
  class MCOperand;
  class MCSymbol;
  class MachineInstr;
  class MachineModuleInfoMachO;
  class MachineOperand;
  class Mangler;

  /// IA64MCInstLower - This class is used to lower an MachineInstr
  /// into an MCInst.
  class LLVM_LIBRARY_VISIBILITY IA64MCInstLower {
    MCContext &Ctx;
    Mangler &Mang;

    AsmPrinter &Printer;
  public:
    IA64MCInstLower(MCContext &ctx, Mangler &mang, AsmPrinter &printer) :
          Ctx(ctx), Mang(mang), Printer(printer) {}
    void Lower(const MachineInstr *MI, MCInst &OutMI) const;

    MCOperand LowerSymbolOperand(const MachineOperand &MO, MCSymbol *Sym) const;

    MCSymbol *GetGlobalAddressSymbol(const MachineOperand &MO) const;
    MCSymbol *GetExternalSymbolSymbol(const MachineOperand &MO) const;
    MCSymbol *GetJumpTableSymbol(const MachineOperand &MO) const;
    MCSymbol *GetConstantPoolIndexSymbol(const MachineOperand &MO) const;
    MCSymbol *GetBlockAddressSymbol(const MachineOperand &MO) const;
  };

} // namespace llvm

#endif // LLVM_TARGET_IA64_MCINSTLOWER_H
