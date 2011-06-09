#include "IA64MCAsmInfo.h"

using namespace llvm;

IA64MCAsmInfo::IA64MCAsmInfo(const Target &T, StringRef TT) {
  AsciiDirective = "\tstring\t";
  AscizDirective = "\tstringz\t";
  CommentString = "//";
  Data8bitsDirective = "\tdata1\t";
  Data16bitsDirective = "\tdata2\t";
  Data32bitsDirective = "\tdata4\t";
  Data64bitsDirective = "\tdata8\t";
  ZeroDirective = NULL;
}
