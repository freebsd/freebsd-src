#include "IA64TargetMachine.h"

using namespace llvm;

IA64SelectionDAGInfo::IA64SelectionDAGInfo(const IA64TargetMachine &TM) :
    TargetSelectionDAGInfo(TM)
{
}

IA64SelectionDAGInfo::~IA64SelectionDAGInfo()
{
}
