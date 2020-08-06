//===-- ARM.h -------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ABIARM.h"
#ifdef LLDB_ENABLE_ALL
#include "ABIMacOSX_arm.h"
#endif // LLDB_ENABLE_ALL
#include "ABISysV_arm.h"
#include "lldb/Core/PluginManager.h"

LLDB_PLUGIN_DEFINE(ABIARM)

void ABIARM::Initialize() {
  ABISysV_arm::Initialize();
#ifdef LLDB_ENABLE_ALL
  ABIMacOSX_arm::Initialize();
#endif // LLDB_ENABLE_ALL
}

void ABIARM::Terminate() {
  ABISysV_arm::Terminate();
#ifdef LLDB_ENABLE_ALL
  ABIMacOSX_arm::Terminate();
#endif // LLDB_ENABLE_ALL
}
