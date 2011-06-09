#include "IA64.h"

#include "llvm/Module.h"
#include "llvm/Type.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/DwarfWriter.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/Target/TargetRegistry.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/ADT/Statistic.h"

using namespace llvm;

namespace {
  class IA64AsmPrinter : public AsmPrinter {
    std::set<std::string> ExternalFunctionNames, ExternalObjectNames;
  public:
    explicit IA64AsmPrinter(formatted_raw_ostream &O, TargetMachine &TM,
	    const MCAsmInfo *T, bool V) :
        AsmPrinter(O, TM, T, V) {}

    virtual const char *getPassName() const {
      return "IA64 Assembly Printer";
    }

    static const char *getRegisterName(unsigned RegNo);

    void PrintGlobalVariable(const GlobalVariable *GVar);

    void printInstruction(const MachineInstr *MI);
    bool runOnMachineFunction(MachineFunction &F);
  };
} // end of anonymous namespace

#include "IA64GenAsmWriter.inc"

void
IA64AsmPrinter::PrintGlobalVariable(const GlobalVariable *GVar)
{
  llvm_unreachable(__func__);
}

bool
IA64AsmPrinter::runOnMachineFunction(MachineFunction &MF)
{
  llvm_unreachable(__func__);
}


extern "C" void
LLVMInitializeIA64AsmPrinter()
{
  RegisterAsmPrinter<IA64AsmPrinter> X(TheIA64Target);
}
