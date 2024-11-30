## @file
#  This python script update content from mipi_syst.h.in in mipi sys-T submodule
#  and generate it as mipi_syst.h. mipi_syst.h include necessary data structure and
#  definition that will be consumed by MipiSysTLib itself, mipi sys-T submodule
#  and other library.
#
#  This script needs to be done once by a developer when adding some
#  project-relating definition or a new version of mipi_syst.h.in is released.
#  Normal users do not need to do this, since the resulting file is stored
#  in the EDK2 git repository.
#
#  Customize structures mentioned below to generate updated mipi_syst.h file:
#  1. ExistingValueToBeReplaced
#       -> To replace existing value in mipi_syst.h.in to newer one.
#  2. ExistingDefinitionToBeRemoved
#       -> To #undef a existing definition in mipi_syst.h.in.
#  3. NewItemToBeAdded
#       -> Items in this structure will be placed at the end of mipi_syst.h as a customized section.
#
#  Run GenMipiSystH.py without any parameters as normal python script after customizing.
#
#  Copyright (c) 2023, Intel Corporation. All rights reserved.<BR>
#
#  SPDX-License-Identifier: BSD-2-Clause-Patent
#
##
import os
import re

#
# A existing value to be customized should place this structure
# Definitions in this customizable structure will be processed by ReplaceOldValue()
# e.g:
#   Before: @SYST_CFG_VERSION_MAJOR@
#   After: 1
#
ExistingValueToBeReplaced = [
    ["@SYST_CFG_VERSION_MAJOR@", "1"],      # Major version
    ["@SYST_CFG_VERSION_MINOR@", "0"],      # Minor version
    ["@SYST_CFG_VERSION_PATCH@", "0"],      # Patch version
    ["@SYST_CFG_CONFORMANCE_LEVEL@", "30"], # Feature level of mipi sys-T submodule
    ["mipi_syst/platform.h", "Platform.h"],
]

#
# A existing definition to be removed should place this structure
# Definitions in this customizable structure will be processed by RemoveDefinition()
# e.g:
#   Before:
#       #define MIPI_SYST_PCFG_ENABLE_PLATFORM_STATE_DATA
#   After:
#       #define MIPI_SYST_PCFG_ENABLE_PLATFORM_STATE_DATA
#       #undef MIPI_SYST_PCFG_ENABLE_PLATFORM_STATE_DATA
#
ExistingDefinitionToBeRemoved = [
    "MIPI_SYST_PCFG_ENABLE_PLATFORM_STATE_DATA",
    "MIPI_SYST_PCFG_ENABLE_HEAP_MEMORY",
    "MIPI_SYST_PCFG_ENABLE_PRINTF_API",
    "MIPI_SYST_PCFG_ENABLE_LOCATION_RECORD",
    "MIPI_SYST_PCFG_ENABLE_LOCATION_ADDRESS",
]

#
# Items in this structure will be placed at the end of mipi_syst.h as a customized section.
#
NewItemToBeAdded = [
    "typedef struct mipi_syst_handle_flags MIPI_SYST_HANDLE_FLAGS;",
    "typedef struct mipi_syst_msg_tag MIPI_SYST_MSG_TAG;",
    "typedef struct mipi_syst_guid MIPI_SYST_GUID;",
    "typedef enum mipi_syst_severity MIPI_SYST_SEVERITY;",
    "typedef struct mipi_syst_handle MIPI_SYST_HANDLE;",
    "typedef struct mipi_syst_header MIPI_SYST_HEADER;",
]

def ProcessSpecialCharacter(Str):
    Str = Str.rstrip(" \n")
    Str = Str.replace("\t", "  ")
    Str += "\n"
    return Str

def ReplaceOldValue(Str):
    for i in range(len(ExistingValueToBeReplaced)):
        Result = re.search(ExistingValueToBeReplaced[i][0], Str)
        if Result is not None:
            Str = Str.replace(ExistingValueToBeReplaced[i][0], ExistingValueToBeReplaced[i][1])
            break
    return Str

def RemoveDefinition(Str):
    Result = re.search("\*", Str)
    if Result is None:
        for i in range(len(ExistingDefinitionToBeRemoved)):
            Result = re.search(ExistingDefinitionToBeRemoved[i], Str)
            if Result is not None:
                Result = re.search("defined", Str)
                if Result is None:
                    Str = Str + "#undef " + ExistingDefinitionToBeRemoved[i]
                    break
    return Str

def main():
    MipiSystHSrcDir = "mipisyst/library/include/mipi_syst.h.in"
    MipiSystHRealSrcDir = os.path.join(os.getcwd(), os.path.normpath(MipiSystHSrcDir))
    MipiSystHRealDstDir = os.path.join(os.getcwd(), "mipi_syst.h")

    #
    # Read content from mipi_syst.h.in and process each line by demand
    #
    with open(MipiSystHRealSrcDir, "r") as rfObj:
        SrcFile = rfObj.readlines()
        for lineIndex in range(len(SrcFile)):
            SrcFile[lineIndex] = ProcessSpecialCharacter(SrcFile[lineIndex])
            SrcFile[lineIndex] = ReplaceOldValue(SrcFile[lineIndex])
            SrcFile[lineIndex] = RemoveDefinition(SrcFile[lineIndex])

    #
    # Typedef a structure or enum type
    #
    i = -1
    for struct in NewItemToBeAdded:
        struct += "\n"
        SrcFile.insert(i, struct)
        i -= 1

    #
    # Save edited content to mipi_syst.h
    #
    with open(MipiSystHRealDstDir, "w") as wfObj:
        wfObj.writelines(SrcFile)

if __name__ == '__main__':
    main()
