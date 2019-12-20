//===-- SystemInitializerFull.cpp -------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "SystemInitializerFull.h"

#include "lldb/API/SBCommandInterpreter.h"

#if !defined(LLDB_DISABLE_PYTHON)
#include "Plugins/ScriptInterpreter/Python/ScriptInterpreterPython.h"
#endif

#include "lldb/Core/Debugger.h"
#include "lldb/Host/Host.h"
#include "lldb/Initialization/SystemInitializerCommon.h"
#include "lldb/Interpreter/CommandInterpreter.h"
#include "lldb/Symbol/ClangASTContext.h"
#include "lldb/Utility/Timer.h"

#ifdef LLDB_ENABLE_ALL
#include "Plugins/ABI/MacOSX-arm/ABIMacOSX_arm.h"
#include "Plugins/ABI/MacOSX-arm64/ABIMacOSX_arm64.h"
#include "Plugins/ABI/MacOSX-i386/ABIMacOSX_i386.h"
#endif // LLDB_ENABLE_ALL
#include "Plugins/ABI/SysV-arm/ABISysV_arm.h"
#include "Plugins/ABI/SysV-arm64/ABISysV_arm64.h"
#ifdef LLDB_ENABLE_ALL
#include "Plugins/ABI/SysV-hexagon/ABISysV_hexagon.h"
#endif // LLDB_ENABLE_ALL
#include "Plugins/ABI/SysV-i386/ABISysV_i386.h"
#include "Plugins/ABI/SysV-mips/ABISysV_mips.h"
#include "Plugins/ABI/SysV-mips64/ABISysV_mips64.h"
#include "Plugins/ABI/SysV-ppc/ABISysV_ppc.h"
#include "Plugins/ABI/SysV-ppc64/ABISysV_ppc64.h"
#ifdef LLDB_ENABLE_ALL
#include "Plugins/ABI/SysV-s390x/ABISysV_s390x.h"
#endif // LLDB_ENABLE_ALL
#include "Plugins/ABI/SysV-x86_64/ABISysV_x86_64.h"
#ifdef LLDB_ENABLE_ALL
#include "Plugins/ABI/Windows-x86_64/ABIWindows_x86_64.h"
#endif // LLDB_ENABLE_ALL
#include "Plugins/Architecture/Arm/ArchitectureArm.h"
#include "Plugins/Architecture/Mips/ArchitectureMips.h"
#include "Plugins/Architecture/PPC64/ArchitecturePPC64.h"
#include "Plugins/Disassembler/llvm/DisassemblerLLVMC.h"
#ifdef LLDB_ENABLE_ALL
#include "Plugins/DynamicLoader/MacOSX-DYLD/DynamicLoaderMacOS.h"
#include "Plugins/DynamicLoader/MacOSX-DYLD/DynamicLoaderMacOSXDYLD.h"
#endif // LLDB_ENABLE_ALL
#include "Plugins/DynamicLoader/POSIX-DYLD/DynamicLoaderPOSIXDYLD.h"
#include "Plugins/DynamicLoader/Static/DynamicLoaderStatic.h"
#ifdef LLDB_ENABLE_ALL
#include "Plugins/DynamicLoader/Windows-DYLD/DynamicLoaderWindowsDYLD.h"
#endif // LLDB_ENABLE_ALL
#include "Plugins/Instruction/ARM/EmulateInstructionARM.h"
#include "Plugins/Instruction/ARM64/EmulateInstructionARM64.h"
#include "Plugins/Instruction/MIPS/EmulateInstructionMIPS.h"
#include "Plugins/Instruction/MIPS64/EmulateInstructionMIPS64.h"
#include "Plugins/Instruction/PPC64/EmulateInstructionPPC64.h"
#include "Plugins/InstrumentationRuntime/ASan/ASanRuntime.h"
#include "Plugins/InstrumentationRuntime/MainThreadChecker/MainThreadCheckerRuntime.h"
#ifdef LLDB_ENABLE_ALL
#include "Plugins/InstrumentationRuntime/TSan/TSanRuntime.h"
#endif // LLDB_ENABLE_ALL
#include "Plugins/InstrumentationRuntime/UBSan/UBSanRuntime.h"
#include "Plugins/JITLoader/GDB/JITLoaderGDB.h"
#include "Plugins/Language/CPlusPlus/CPlusPlusLanguage.h"
#ifdef LLDB_ENABLE_ALL
#include "Plugins/Language/ObjC/ObjCLanguage.h"
#include "Plugins/Language/ObjCPlusPlus/ObjCPlusPlusLanguage.h"
#endif // LLDB_ENABLE_ALL
#include "Plugins/LanguageRuntime/CPlusPlus/ItaniumABI/ItaniumABILanguageRuntime.h"
#ifdef LLDB_ENABLE_ALL
#include "Plugins/LanguageRuntime/ObjC/AppleObjCRuntime/AppleObjCRuntimeV1.h"
#include "Plugins/LanguageRuntime/ObjC/AppleObjCRuntime/AppleObjCRuntimeV2.h"
#include "Plugins/LanguageRuntime/RenderScript/RenderScriptRuntime/RenderScriptRuntime.h"
#endif // LLDB_ENABLE_ALL
#include "Plugins/MemoryHistory/asan/MemoryHistoryASan.h"
#include "Plugins/ObjectContainer/BSD-Archive/ObjectContainerBSDArchive.h"
#ifdef LLDB_ENABLE_ALL
#include "Plugins/ObjectContainer/Universal-Mach-O/ObjectContainerUniversalMachO.h"
#endif // LLDB_ENABLE_ALL
#include "Plugins/ObjectFile/Breakpad/ObjectFileBreakpad.h"
#include "Plugins/ObjectFile/ELF/ObjectFileELF.h"
#ifdef LLDB_ENABLE_ALL
#include "Plugins/ObjectFile/Mach-O/ObjectFileMachO.h"
#include "Plugins/ObjectFile/PECOFF/ObjectFilePECOFF.h"
#include "Plugins/OperatingSystem/Python/OperatingSystemPython.h"
#include "Plugins/Platform/Android/PlatformAndroid.h"
#endif // LLDB_ENABLE_ALL
#include "Plugins/Platform/FreeBSD/PlatformFreeBSD.h"
#ifdef LLDB_ENABLE_ALL
#include "Plugins/Platform/Linux/PlatformLinux.h"
#include "Plugins/Platform/MacOSX/PlatformMacOSX.h"
#include "Plugins/Platform/MacOSX/PlatformRemoteiOS.h"
#include "Plugins/Platform/NetBSD/PlatformNetBSD.h"
#include "Plugins/Platform/OpenBSD/PlatformOpenBSD.h"
#include "Plugins/Platform/Windows/PlatformWindows.h"
#endif // LLDB_ENABLE_ALL
#include "Plugins/Platform/gdb-server/PlatformRemoteGDBServer.h"
#include "Plugins/Process/elf-core/ProcessElfCore.h"
#include "Plugins/Process/gdb-remote/ProcessGDBRemote.h"
#ifdef LLDB_ENABLE_ALL
#include "Plugins/Process/mach-core/ProcessMachCore.h"
#include "Plugins/Process/minidump/ProcessMinidump.h"
#endif // LLDB_ENABLE_ALL
#include "Plugins/ScriptInterpreter/None/ScriptInterpreterNone.h"
#include "Plugins/SymbolFile/Breakpad/SymbolFileBreakpad.h"
#include "Plugins/SymbolFile/DWARF/SymbolFileDWARF.h"
#include "Plugins/SymbolFile/DWARF/SymbolFileDWARFDebugMap.h"
#ifdef LLDB_ENABLE_ALL
#include "Plugins/SymbolFile/PDB/SymbolFilePDB.h"
#endif // LLDB_ENABLE_ALL
#include "Plugins/SymbolFile/Symtab/SymbolFileSymtab.h"
#include "Plugins/SymbolVendor/ELF/SymbolVendorELF.h"
#ifdef LLDB_ENABLE_ALL
#include "Plugins/SystemRuntime/MacOSX/SystemRuntimeMacOSX.h"
#endif // LLDB_ENABLE_ALL
#include "Plugins/UnwindAssembly/InstEmulation/UnwindAssemblyInstEmulation.h"
#include "Plugins/UnwindAssembly/x86/UnwindAssembly-x86.h"

#if defined(__APPLE__)
#include "Plugins/DynamicLoader/Darwin-Kernel/DynamicLoaderDarwinKernel.h"
#include "Plugins/Platform/MacOSX/PlatformAppleTVSimulator.h"
#include "Plugins/Platform/MacOSX/PlatformAppleWatchSimulator.h"
#include "Plugins/Platform/MacOSX/PlatformDarwinKernel.h"
#include "Plugins/Platform/MacOSX/PlatformRemoteAppleBridge.h"
#include "Plugins/Platform/MacOSX/PlatformRemoteAppleTV.h"
#include "Plugins/Platform/MacOSX/PlatformRemoteAppleWatch.h"
#include "Plugins/Platform/MacOSX/PlatformiOSSimulator.h"
#include "Plugins/Process/MacOSX-Kernel/ProcessKDP.h"
#include "Plugins/SymbolVendor/MacOSX/SymbolVendorMacOSX.h"
#endif
#ifdef LLDB_ENABLE_ALL
#include "Plugins/StructuredData/DarwinLog/StructuredDataDarwinLog.h"
#endif // LLDB_ENABLE_ALL

#if defined(__FreeBSD__)
#include "Plugins/Process/FreeBSD/ProcessFreeBSD.h"
#endif

#if defined(_WIN32)
#include "Plugins/Process/Windows/Common/ProcessWindows.h"
#include "lldb/Host/windows/windows.h"
#endif

#include "llvm/Support/TargetSelect.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#include "llvm/ExecutionEngine/MCJIT.h"
#pragma clang diagnostic pop

#include <string>

using namespace lldb_private;

SystemInitializerFull::SystemInitializerFull() {}

SystemInitializerFull::~SystemInitializerFull() {}

llvm::Error SystemInitializerFull::Initialize() {
  if (auto e = SystemInitializerCommon::Initialize())
    return e;

  breakpad::ObjectFileBreakpad::Initialize();
  ObjectFileELF::Initialize();
#ifdef LLDB_ENABLE_ALL
  ObjectFileMachO::Initialize();
  ObjectFilePECOFF::Initialize();
#endif // LLDB_ENABLE_ALL

  ObjectContainerBSDArchive::Initialize();
#ifdef LLDB_ENABLE_ALL
  ObjectContainerUniversalMachO::Initialize();
#endif // LLDB_ENABLE_ALL

  ScriptInterpreterNone::Initialize();

#ifndef LLDB_DISABLE_PYTHON
  OperatingSystemPython::Initialize();
#endif

#if !defined(LLDB_DISABLE_PYTHON)
  ScriptInterpreterPython::Initialize();
#endif

  platform_freebsd::PlatformFreeBSD::Initialize();
#ifdef LLDB_ENABLE_ALL
  platform_linux::PlatformLinux::Initialize();
  platform_netbsd::PlatformNetBSD::Initialize();
  platform_openbsd::PlatformOpenBSD::Initialize();
  PlatformWindows::Initialize();
  platform_android::PlatformAndroid::Initialize();
  PlatformRemoteiOS::Initialize();
  PlatformMacOSX::Initialize();
#endif // LLDB_ENABLE_ALL
#if defined(__APPLE__)
  PlatformiOSSimulator::Initialize();
  PlatformDarwinKernel::Initialize();
#endif

  // Initialize LLVM and Clang
  llvm::InitializeAllTargets();
  llvm::InitializeAllAsmPrinters();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllDisassemblers();

  ClangASTContext::Initialize();

#ifdef LLDB_ENABLE_ALL
  ABIMacOSX_i386::Initialize();
  ABIMacOSX_arm::Initialize();
  ABIMacOSX_arm64::Initialize();
#endif // LLDB_ENABLE_ALL
  ABISysV_arm::Initialize();
  ABISysV_arm64::Initialize();
#ifdef LLDB_ENABLE_ALL
  ABISysV_hexagon::Initialize();
#endif // LLDB_ENABLE_ALL
  ABISysV_i386::Initialize();
  ABISysV_x86_64::Initialize();
  ABISysV_ppc::Initialize();
  ABISysV_ppc64::Initialize();
  ABISysV_mips::Initialize();
  ABISysV_mips64::Initialize();
#ifdef LLDB_ENABLE_ALL
  ABISysV_s390x::Initialize();
  ABIWindows_x86_64::Initialize();
#endif // LLDB_ENABLE_ALL

  ArchitectureArm::Initialize();
  ArchitectureMips::Initialize();
  ArchitecturePPC64::Initialize();

  DisassemblerLLVMC::Initialize();

  JITLoaderGDB::Initialize();
  ProcessElfCore::Initialize();
#ifdef LLDB_ENABLE_ALL
  ProcessMachCore::Initialize();
  minidump::ProcessMinidump::Initialize();
#endif // LLDB_ENABLE_ALL
  MemoryHistoryASan::Initialize();
  AddressSanitizerRuntime::Initialize();
#ifdef LLDB_ENABLE_ALL
  ThreadSanitizerRuntime::Initialize();
#endif // LLDB_ENABLE_ALL
  UndefinedBehaviorSanitizerRuntime::Initialize();
  MainThreadCheckerRuntime::Initialize();

  SymbolVendorELF::Initialize();
  breakpad::SymbolFileBreakpad::Initialize();
  SymbolFileDWARF::Initialize();
#ifdef LLDB_ENABLE_ALL
  SymbolFilePDB::Initialize();
#endif // LLDB_ENABLE_ALL
  SymbolFileSymtab::Initialize();
  UnwindAssemblyInstEmulation::Initialize();
  UnwindAssembly_x86::Initialize();

  EmulateInstructionARM::Initialize();
  EmulateInstructionARM64::Initialize();
  EmulateInstructionMIPS::Initialize();
  EmulateInstructionMIPS64::Initialize();
  EmulateInstructionPPC64::Initialize();

  SymbolFileDWARFDebugMap::Initialize();
  ItaniumABILanguageRuntime::Initialize();
#ifdef LLDB_ENABLE_ALL
  AppleObjCRuntimeV2::Initialize();
  AppleObjCRuntimeV1::Initialize();
  SystemRuntimeMacOSX::Initialize();
  RenderScriptRuntime::Initialize();
#endif // LLDB_ENABLE_ALL

  CPlusPlusLanguage::Initialize();
#ifdef LLDB_ENABLE_ALL
  ObjCLanguage::Initialize();
  ObjCPlusPlusLanguage::Initialize();
#endif // LLDB_ENABLE_ALL

#if defined(_WIN32)
  ProcessWindows::Initialize();
#endif
#if defined(__FreeBSD__)
  ProcessFreeBSD::Initialize();
#endif
#if defined(__APPLE__)
  SymbolVendorMacOSX::Initialize();
  ProcessKDP::Initialize();
  PlatformAppleTVSimulator::Initialize();
  PlatformAppleWatchSimulator::Initialize();
  PlatformRemoteAppleTV::Initialize();
  PlatformRemoteAppleWatch::Initialize();
  PlatformRemoteAppleBridge::Initialize();
  DynamicLoaderDarwinKernel::Initialize();
#endif

  // This plugin is valid on any host that talks to a Darwin remote. It
  // shouldn't be limited to __APPLE__.
#ifdef LLDB_ENABLE_ALL
  StructuredDataDarwinLog::Initialize();
#endif // LLDB_ENABLE_ALL

  // Platform agnostic plugins
  platform_gdb_server::PlatformRemoteGDBServer::Initialize();

  process_gdb_remote::ProcessGDBRemote::Initialize();
#ifdef LLDB_ENABLE_ALL
  DynamicLoaderMacOSXDYLD::Initialize();
  DynamicLoaderMacOS::Initialize();
#endif // LLDB_ENABLE_ALL
  DynamicLoaderPOSIXDYLD::Initialize();
  DynamicLoaderStatic::Initialize();
#ifdef LLDB_ENABLE_ALL
  DynamicLoaderWindowsDYLD::Initialize();
#endif // LLDB_ENABLE_ALL

  // Scan for any system or user LLDB plug-ins
  PluginManager::Initialize();

  // The process settings need to know about installed plug-ins, so the
  // Settings must be initialized
  // AFTER PluginManager::Initialize is called.

  Debugger::SettingsInitialize();

  return llvm::Error::success();
}

void SystemInitializerFull::Terminate() {
  static Timer::Category func_cat(LLVM_PRETTY_FUNCTION);
  Timer scoped_timer(func_cat, LLVM_PRETTY_FUNCTION);

  Debugger::SettingsTerminate();

  // Terminate and unload and loaded system or user LLDB plug-ins
  PluginManager::Terminate();

  ClangASTContext::Terminate();

#ifdef LLDB_ENABLE_ALL
  ArchitectureArm::Terminate();
  ArchitectureMips::Terminate();
  ArchitecturePPC64::Terminate();

  ABIMacOSX_i386::Terminate();
  ABIMacOSX_arm::Terminate();
  ABIMacOSX_arm64::Terminate();
#endif // LLDB_ENABLE_ALL
  ABISysV_arm::Terminate();
  ABISysV_arm64::Terminate();
#ifdef LLDB_ENABLE_ALL
  ABISysV_hexagon::Terminate();
#endif // LLDB_ENABLE_ALL
  ABISysV_i386::Terminate();
  ABISysV_x86_64::Terminate();
  ABISysV_ppc::Terminate();
  ABISysV_ppc64::Terminate();
  ABISysV_mips::Terminate();
  ABISysV_mips64::Terminate();
#ifdef LLDB_ENABLE_ALL
  ABISysV_s390x::Terminate();
  ABIWindows_x86_64::Terminate();
#endif // LLDB_ENABLE_ALL
  DisassemblerLLVMC::Terminate();

  JITLoaderGDB::Terminate();
  ProcessElfCore::Terminate();
#ifdef LLDB_ENABLE_ALL
  ProcessMachCore::Terminate();
  minidump::ProcessMinidump::Terminate();
#endif // LLDB_ENABLE_ALL
  MemoryHistoryASan::Terminate();
  AddressSanitizerRuntime::Terminate();
#ifdef LLDB_ENABLE_ALL
  ThreadSanitizerRuntime::Terminate();
#endif // LLDB_ENABLE_ALL
  UndefinedBehaviorSanitizerRuntime::Terminate();
  MainThreadCheckerRuntime::Terminate();
  SymbolVendorELF::Terminate();
  breakpad::SymbolFileBreakpad::Terminate();
  SymbolFileDWARF::Terminate();
#ifdef LLDB_ENABLE_ALL
  SymbolFilePDB::Terminate();
#endif // LLDB_ENABLE_ALL
  SymbolFileSymtab::Terminate();
  UnwindAssembly_x86::Terminate();
  UnwindAssemblyInstEmulation::Terminate();

  EmulateInstructionARM::Terminate();
  EmulateInstructionARM64::Terminate();
  EmulateInstructionMIPS::Terminate();
  EmulateInstructionMIPS64::Terminate();
  EmulateInstructionPPC64::Terminate();

  SymbolFileDWARFDebugMap::Terminate();
  ItaniumABILanguageRuntime::Terminate();
#ifdef LLDB_ENABLE_ALL
  AppleObjCRuntimeV2::Terminate();
  AppleObjCRuntimeV1::Terminate();
  SystemRuntimeMacOSX::Terminate();
  RenderScriptRuntime::Terminate();
#endif // LLDB_ENABLE_ALL

  CPlusPlusLanguage::Terminate();
#ifdef LLDB_ENABLE_ALL
  ObjCLanguage::Terminate();
  ObjCPlusPlusLanguage::Terminate();
#endif // LLDB_ENABLE_ALL

#if defined(__APPLE__)
  DynamicLoaderDarwinKernel::Terminate();
  ProcessKDP::Terminate();
  SymbolVendorMacOSX::Terminate();
  PlatformAppleTVSimulator::Terminate();
  PlatformAppleWatchSimulator::Terminate();
  PlatformRemoteAppleTV::Terminate();
  PlatformRemoteAppleWatch::Terminate();
  PlatformRemoteAppleBridge::Terminate();
#endif

#if defined(__FreeBSD__)
  ProcessFreeBSD::Terminate();
#endif
  Debugger::SettingsTerminate();

  platform_gdb_server::PlatformRemoteGDBServer::Terminate();
  process_gdb_remote::ProcessGDBRemote::Terminate();
#ifdef LLDB_ENABLE_ALL
  StructuredDataDarwinLog::Terminate();

  DynamicLoaderMacOSXDYLD::Terminate();
  DynamicLoaderMacOS::Terminate();
#endif // LLDB_ENABLE_ALL
  DynamicLoaderPOSIXDYLD::Terminate();
  DynamicLoaderStatic::Terminate();
#ifdef LLDB_ENABLE_ALL
  DynamicLoaderWindowsDYLD::Terminate();
#endif // LLDB_ENABLE_ALL

#ifndef LLDB_DISABLE_PYTHON
  OperatingSystemPython::Terminate();
#endif

  platform_freebsd::PlatformFreeBSD::Terminate();
#ifdef LLDB_ENABLE_ALL
  platform_linux::PlatformLinux::Terminate();
  platform_netbsd::PlatformNetBSD::Terminate();
  platform_openbsd::PlatformOpenBSD::Terminate();
  PlatformWindows::Terminate();
  platform_android::PlatformAndroid::Terminate();
  PlatformMacOSX::Terminate();
  PlatformRemoteiOS::Terminate();
#endif // LLDB_ENABLE_ALL
#if defined(__APPLE__)
  PlatformiOSSimulator::Terminate();
  PlatformDarwinKernel::Terminate();
#endif

  breakpad::ObjectFileBreakpad::Terminate();
  ObjectFileELF::Terminate();
#ifdef LLDB_ENABLE_ALL
  ObjectFileMachO::Terminate();
  ObjectFilePECOFF::Terminate();
#endif // LLDB_ENABLE_ALL

  ObjectContainerBSDArchive::Terminate();
#ifdef LLDB_ENABLE_ALL
  ObjectContainerUniversalMachO::Terminate();
#endif // LLDB_ENABLE_ALL

  // Now shutdown the common parts, in reverse order.
  SystemInitializerCommon::Terminate();
}
