#include "IA64.h"
#include "IA64InstrInfo.h"
#include "IA64MCAsmInfo.h"
#include "IA64MCInstLower.h"
#include "IA64TargetMachine.h"
#include "InstPrinter/IA64InstPrinter.h"

#include "llvm/Constants.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Module.h"
#include "llvm/Assembly/Writer.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineConstantPool.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/Target/Mangler.h"
#include "llvm/Target/TargetData.h"
#include "llvm/Target/TargetLoweringObjectFile.h"
#include "llvm/Target/TargetRegistry.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {

  class IA64AsmPrinter : public AsmPrinter {
  public:
    IA64AsmPrinter(TargetMachine &TM, MCStreamer &Streamer) :
        AsmPrinter(TM, Streamer) {}

    virtual const char *getPassName() const {
      return "IA-64 Assembly Printer";
    }

    void printOperand(const MachineInstr *MI, int OpNum, raw_ostream &O,
          const char* Modifier = 0);
    void printSrcMemOperand(const MachineInstr *MI, int OpNum, raw_ostream &O);
    bool PrintAsmOperand(const MachineInstr *MI, unsigned OpNo,
          unsigned AsmVariant, const char *ExtraCode, raw_ostream &O);
    bool PrintAsmMemoryOperand(const MachineInstr *MI, unsigned OpNo,
          unsigned AsmVariant, const char *ExtraCode, raw_ostream &O);
    void EmitInstruction(const MachineInstr *MI);
  };

} // end of anonymous namespace

void
IA64AsmPrinter::printOperand(const MachineInstr *MI, int OpNum,
      raw_ostream &O, const char *Modifier)
{
  llvm_unreachable(__func__);
}

void
IA64AsmPrinter::printSrcMemOperand(const MachineInstr *MI, int OpNum,
      raw_ostream &O)
{
  const MachineOperand &Base = MI->getOperand(OpNum);
  const MachineOperand &Disp = MI->getOperand(OpNum+1);

  // Print displacement first

  // Imm here is in fact global address - print extra modifier.
  if (Disp.isImm() && !Base.getReg())
    O << '&';
  printOperand(MI, OpNum+1, O, "nohash");

  // Print register base field
  if (Base.getReg()) {
    O << '(';
    printOperand(MI, OpNum, O);
    O << ')';
  }
}

/// PrintAsmOperand - Print out an operand for an inline asm expression.
bool
IA64AsmPrinter::PrintAsmOperand(const MachineInstr *MI, unsigned OpNo,
      unsigned AsmVariant, const char *ExtraCode, raw_ostream &O)
{
  // Does this asm operand have a single letter operand modifier?
  if (ExtraCode && ExtraCode[0])
    return true; // Unknown modifier.

  printOperand(MI, OpNo, O);
  return false;
}

bool
IA64AsmPrinter::PrintAsmMemoryOperand(const MachineInstr *MI, unsigned OpNo,
      unsigned AsmVariant, const char *ExtraCode, raw_ostream &O)
{
  if (ExtraCode && ExtraCode[0]) {
    return true; // Unknown modifier.
  }
  printSrcMemOperand(MI, OpNo, O);
  return false;
}

void
IA64AsmPrinter::EmitInstruction(const MachineInstr *MI)
{
  IA64MCInstLower MCInstLowering(OutContext, *Mang, *this);

  MCInst TmpInst;
  MCInstLowering.Lower(MI, TmpInst);
  OutStreamer.EmitInstruction(TmpInst);
}

static
MCInstPrinter *createIA64MCInstPrinter(const Target &T, TargetMachine &TM,
      unsigned SyntaxVariant, const MCAsmInfo &MAI)
{
  if (SyntaxVariant == 0)
    return new IA64InstPrinter(TM, MAI);
  return 0;
}

// Force static initialization.
extern "C" void
LLVMInitializeIA64AsmPrinter()
{
  RegisterAsmPrinter<IA64AsmPrinter> X(TheIA64Target);
  TargetRegistry::RegisterMCInstPrinter(TheIA64Target,
        createIA64MCInstPrinter);
}
