## @file
# EFI/PI MdePkg Package
#
# Copyright (c) 2007 - 2022, Intel Corporation. All rights reserved.<BR>
# Portions copyright (c) 2008 - 2009, Apple Inc. All rights reserved.<BR>
# (C) Copyright 2020 Hewlett Packard Enterprise Development LP<BR>
# Copyright (c) 2022, Loongson Technology Corporation Limited. All rights reserved.<BR>
#
#    SPDX-License-Identifier: BSD-2-Clause-Patent
#
##

[Defines]
  PLATFORM_NAME                  = Mde
  PLATFORM_GUID                  = 082F8BFC-0455-4859-AE3C-ECD64FB81642
  PLATFORM_VERSION               = 1.08
  DSC_SPECIFICATION              = 0x00010005
  OUTPUT_DIRECTORY               = Build/Mde
  SUPPORTED_ARCHITECTURES        = IA32|X64|EBC|ARM|AARCH64|RISCV64|LOONGARCH64
  BUILD_TARGETS                  = DEBUG|RELEASE|NOOPT
  SKUID_IDENTIFIER               = DEFAULT

!include UnitTestFrameworkPkg/UnitTestFrameworkPkgTarget.dsc.inc

!include MdePkg/MdeLibs.dsc.inc

[PcdsFeatureFlag]
  gEfiMdePkgTokenSpaceGuid.PcdUgaConsumeSupport|TRUE

[PcdsFixedAtBuild]
  gEfiMdePkgTokenSpaceGuid.PcdDebugPropertyMask|0x0f
  gEfiMdePkgTokenSpaceGuid.PcdDebugPrintErrorLevel|0x80000000
  gEfiMdePkgTokenSpaceGuid.PcdPciExpressBaseAddress|0xE0000000

[LibraryClasses]
  SafeIntLib|MdePkg/Library/BaseSafeIntLib/BaseSafeIntLib.inf

[Components]
  MdePkg/Library/UefiFileHandleLib/UefiFileHandleLib.inf
  MdePkg/Library/BaseCacheMaintenanceLib/BaseCacheMaintenanceLib.inf
  MdePkg/Library/BaseCacheMaintenanceLibNull/BaseCacheMaintenanceLibNull.inf
  MdePkg/Library/BaseCpuLib/BaseCpuLib.inf
  MdePkg/Library/BaseCpuLibNull/BaseCpuLibNull.inf
  MdePkg/Library/BaseDebugLibNull/BaseDebugLibNull.inf
  MdePkg/Library/BaseDebugLibSerialPort/BaseDebugLibSerialPort.inf
  MdePkg/Library/BaseDebugPrintErrorLevelLib/BaseDebugPrintErrorLevelLib.inf
  MdePkg/Library/BaseLib/BaseLib.inf
  MdePkg/Library/BaseMemoryLib/BaseMemoryLib.inf
  MdePkg/Library/BaseOrderedCollectionRedBlackTreeLib/BaseOrderedCollectionRedBlackTreeLib.inf
  MdePkg/Library/BasePcdLibNull/BasePcdLibNull.inf
  MdePkg/Library/BasePciCf8Lib/BasePciCf8Lib.inf
  MdePkg/Library/BasePciExpressLib/BasePciExpressLib.inf
  MdePkg/Library/BasePciLibCf8/BasePciLibCf8.inf
  MdePkg/Library/BasePciLibPciExpress/BasePciLibPciExpress.inf
  MdePkg/Library/BasePciSegmentLibPci/BasePciSegmentLibPci.inf
  MdePkg/Library/BasePciSegmentInfoLibNull/BasePciSegmentInfoLibNull.inf
  MdePkg/Library/PciSegmentLibSegmentInfo/BasePciSegmentLibSegmentInfo.inf
  MdePkg/Library/PciSegmentLibSegmentInfo/DxeRuntimePciSegmentLibSegmentInfo.inf
  MdePkg/Library/BaseS3PciSegmentLib/BaseS3PciSegmentLib.inf
  MdePkg/Library/BaseArmTrngLibNull/BaseArmTrngLibNull.inf
  MdePkg/Library/BasePeCoffGetEntryPointLib/BasePeCoffGetEntryPointLib.inf
  MdePkg/Library/BasePeCoffLib/BasePeCoffLib.inf
  MdePkg/Library/BasePeCoffExtraActionLibNull/BasePeCoffExtraActionLibNull.inf
  MdePkg/Library/BasePerformanceLibNull/BasePerformanceLibNull.inf
  MdePkg/Library/BasePostCodeLibDebug/BasePostCodeLibDebug.inf
  MdePkg/Library/BasePostCodeLibPort80/BasePostCodeLibPort80.inf
  MdePkg/Library/BasePrintLib/BasePrintLib.inf
  MdePkg/Library/BaseReportStatusCodeLibNull/BaseReportStatusCodeLibNull.inf
  MdePkg/Library/DxeRngLib/DxeRngLib.inf
  MdePkg/Library/BaseRngLibNull/BaseRngLibNull.inf
  MdePkg/Library/BaseRngLibTimerLib/BaseRngLibTimerLib.inf

  MdePkg/Library/BaseSerialPortLibNull/BaseSerialPortLibNull.inf
  MdePkg/Library/BaseSynchronizationLib/BaseSynchronizationLib.inf
  MdePkg/Library/BaseTimerLibNullTemplate/BaseTimerLibNullTemplate.inf
  MdePkg/Library/BaseUefiDecompressLib/BaseUefiDecompressLib.inf
  MdePkg/Library/BaseUefiDecompressLib/BaseUefiTianoCustomDecompressLib.inf
  MdePkg/Library/BaseSmbusLibNull/BaseSmbusLibNull.inf
  MdePkg/Library/BaseSafeIntLib/BaseSafeIntLib.inf

  MdePkg/Library/DxeCoreEntryPoint/DxeCoreEntryPoint.inf
  MdePkg/Library/DxeCoreHobLib/DxeCoreHobLib.inf
  MdePkg/Library/DxeExtractGuidedSectionLib/DxeExtractGuidedSectionLib.inf
  MdePkg/Library/DxeHobLib/DxeHobLib.inf
  MdePkg/Library/DxePcdLib/DxePcdLib.inf
  MdePkg/Library/DxeServicesLib/DxeServicesLib.inf
  MdePkg/Library/DxeServicesTableLib/DxeServicesTableLib.inf
  MdePkg/Library/DxeSmbusLib/DxeSmbusLib.inf
  MdePkg/Library/DxeIoLibCpuIo2/DxeIoLibCpuIo2.inf
  MdePkg/Library/DxeHstiLib/DxeHstiLib.inf
  MdePkg/Library/DxeRuntimePciExpressLib/DxeRuntimePciExpressLib.inf
  MdePkg/Library/DxeRuntimeDebugLibSerialPort/DxeRuntimeDebugLibSerialPort.inf

  MdePkg/Library/PeiCoreEntryPoint/PeiCoreEntryPoint.inf
  MdePkg/Library/PeiDxePostCodeLibReportStatusCode/PeiDxePostCodeLibReportStatusCode.inf
  MdePkg/Library/PeiExtractGuidedSectionLib/PeiExtractGuidedSectionLib.inf
  MdePkg/Library/PeiHobLib/PeiHobLib.inf
  MdePkg/Library/PeiIoLibCpuIo/PeiIoLibCpuIo.inf
  MdePkg/Library/PeiMemoryAllocationLib/PeiMemoryAllocationLib.inf
  MdePkg/Library/PeiMemoryLib/PeiMemoryLib.inf
  MdePkg/Library/PeimEntryPoint/PeimEntryPoint.inf
  MdePkg/Library/PeiPcdLib/PeiPcdLib.inf
  MdePkg/Library/PeiResourcePublicationLib/PeiResourcePublicationLib.inf
  MdePkg/Library/PeiServicesLib/PeiServicesLib.inf
  MdePkg/Library/PeiServicesTablePointerLib/PeiServicesTablePointerLib.inf
  MdePkg/Library/PeiSmbusLibSmbus2Ppi/PeiSmbusLibSmbus2Ppi.inf
  MdePkg/Library/PeiPciLibPciCfg2/PeiPciLibPciCfg2.inf
  MdePkg/Library/PeiPciSegmentLibPciCfg2/PeiPciSegmentLibPciCfg2.inf

  MdePkg/Library/UefiApplicationEntryPoint/UefiApplicationEntryPoint.inf
  MdePkg/Library/UefiBootServicesTableLib/UefiBootServicesTableLib.inf
  MdePkg/Library/UefiDebugLibConOut/UefiDebugLibConOut.inf
  MdePkg/Library/UefiDebugLibDebugPortProtocol/UefiDebugLibDebugPortProtocol.inf
  MdePkg/Library/UefiDebugLibStdErr/UefiDebugLibStdErr.inf
  MdePkg/Library/UefiDevicePathLib/UefiDevicePathLib.inf
  MdePkg/Library/UefiDevicePathLib/UefiDevicePathLibBase.inf
  MdePkg/Library/UefiDevicePathLib/UefiDevicePathLibStandaloneMm.inf
  MdePkg/Library/UefiDevicePathLib/UefiDevicePathLibOptionalDevicePathProtocol.inf
  MdePkg/Library/UefiDevicePathLibDevicePathProtocol/UefiDevicePathLibDevicePathProtocol.inf
  MdePkg/Library/UefiDriverEntryPoint/UefiDriverEntryPoint.inf
  MdePkg/Library/UefiLib/UefiLib.inf
  MdePkg/Library/UefiMemoryAllocationLib/UefiMemoryAllocationLib.inf
  MdePkg/Library/UefiMemoryLib/UefiMemoryLib.inf
  MdePkg/Library/UefiRuntimeServicesTableLib/UefiRuntimeServicesTableLib.inf
  MdePkg/Library/UefiScsiLib/UefiScsiLib.inf
  MdePkg/Library/UefiUsbLib/UefiUsbLib.inf
  MdePkg/Library/UefiPciLibPciRootBridgeIo/UefiPciLibPciRootBridgeIo.inf
  MdePkg/Library/UefiPciSegmentLibPciRootBridgeIo/UefiPciSegmentLibPciRootBridgeIo.inf
  MdePkg/Library/SmmLibNull/SmmLibNull.inf
  MdePkg/Library/BaseExtractGuidedSectionLib/BaseExtractGuidedSectionLib.inf

  MdePkg/Library/StandaloneMmDriverEntryPoint/StandaloneMmDriverEntryPoint.inf
  MdePkg/Library/StandaloneMmServicesTableLib/StandaloneMmServicesTableLib.inf

  MdePkg/Library/RegisterFilterLibNull/RegisterFilterLibNull.inf
  MdePkg/Library/CcProbeLibNull/CcProbeLibNull.inf
  MdePkg/Library/SmmCpuRendezvousLibNull/SmmCpuRendezvousLibNull.inf

  MdePkg/Library/JedecJep106Lib/JedecJep106Lib.inf
  MdePkg/Library/BaseFdtLib/BaseFdtLib.inf
  MdePkg/Library/PeiRngLib/PeiRngLib.inf

  MdePkg/Library/StackCheckFailureHookLibNull/StackCheckFailureHookLibNull.inf
  MdePkg/Library/StackCheckLibNull/StackCheckLibNull.inf
  MdePkg/Library/StackCheckLib/StackCheckLib.inf
  MdePkg/Library/DynamicStackCookieEntryPointLib/DxeCoreEntryPoint.inf
  MdePkg/Library/DynamicStackCookieEntryPointLib/StandaloneMmDriverEntryPoint.inf
  MdePkg/Library/DynamicStackCookieEntryPointLib/UefiApplicationEntryPoint.inf
  MdePkg/Library/DynamicStackCookieEntryPointLib/UefiDriverEntryPoint.inf

[Components.IA32, Components.X64, Components.ARM, Components.AARCH64]
  #
  # Add UEFI Target Based Unit Tests
  #
  MdePkg/Test/UnitTest/Library/BaseLib/BaseLibUnitTestsUefi.inf

  #
  # Build PEIM, DXE_DRIVER, SMM_DRIVER, UEFI Shell components that test SafeIntLib
  #
  MdePkg/Test/UnitTest/Library/BaseSafeIntLib/TestBaseSafeIntLibPei.inf
  MdePkg/Test/UnitTest/Library/BaseSafeIntLib/TestBaseSafeIntLibDxe.inf
  MdePkg/Test/UnitTest/Library/BaseSafeIntLib/TestBaseSafeIntLibSmm.inf
  MdePkg/Test/UnitTest/Library/BaseSafeIntLib/TestBaseSafeIntLibUefiShell.inf

[Components.IA32, Components.X64, Components.AARCH64]
  MdePkg/Library/BaseRngLib/BaseRngLib.inf

[Components.IA32, Components.X64]
  MdePkg/Library/BaseIoLibIntrinsic/BaseIoLibIntrinsic.inf
  MdePkg/Library/BaseIoLibIntrinsic/BaseIoLibIntrinsicSev.inf
  MdePkg/Library/BaseMemoryLibMmx/BaseMemoryLibMmx.inf
  MdePkg/Library/BaseMemoryLibOptDxe/BaseMemoryLibOptDxe.inf
  MdePkg/Library/BaseMemoryLibOptPei/BaseMemoryLibOptPei.inf
  MdePkg/Library/BaseMemoryLibRepStr/BaseMemoryLibRepStr.inf
  MdePkg/Library/BaseMemoryLibSse2/BaseMemoryLibSse2.inf
  MdePkg/Library/PeiServicesTablePointerLibIdt/PeiServicesTablePointerLibIdt.inf
  MdePkg/Library/SecPeiDxeTimerLibCpu/SecPeiDxeTimerLibCpu.inf
  MdePkg/Library/UefiRuntimeLib/UefiRuntimeLib.inf
  MdePkg/Library/SmmIoLibSmmCpuIo2/SmmIoLibSmmCpuIo2.inf
  MdePkg/Library/SmmPciLibPciRootBridgeIo/SmmPciLibPciRootBridgeIo.inf
  MdePkg/Library/SmmServicesTableLib/SmmServicesTableLib.inf
  MdePkg/Library/SmmMemoryAllocationLib/SmmMemoryAllocationLib.inf
  MdePkg/Library/SmmPeriodicSmiLib/SmmPeriodicSmiLib.inf
  MdePkg/Library/BaseS3BootScriptLibNull/BaseS3BootScriptLibNull.inf
  MdePkg/Library/BaseS3IoLib/BaseS3IoLib.inf
  MdePkg/Library/BaseS3PciLib/BaseS3PciLib.inf
  MdePkg/Library/BaseS3SmbusLib/BaseS3SmbusLib.inf
  MdePkg/Library/BaseS3StallLib/BaseS3StallLib.inf
  MdePkg/Library/SmmMemLib/SmmMemLib.inf
  MdePkg/Library/SmmIoLib/SmmIoLib.inf
  MdePkg/Library/SmmPciExpressLib/SmmPciExpressLib.inf
  MdePkg/Library/SmiHandlerProfileLibNull/SmiHandlerProfileLibNull.inf
  MdePkg/Library/MmServicesTableLib/MmServicesTableLib.inf
  MdePkg/Library/MmUnblockMemoryLib/MmUnblockMemoryLibNull.inf
  MdePkg/Library/TdxLib/TdxLib.inf
  MdePkg/Library/MipiSysTLib/MipiSysTLib.inf
  MdePkg/Library/TraceHubDebugSysTLibNull/TraceHubDebugSysTLibNull.inf

[Components.X64]
  MdePkg/Library/DynamicStackCookieEntryPointLib/StandaloneMmCoreEntryPoint.inf
  MdePkg/Library/StandaloneMmCoreEntryPoint/StandaloneMmCoreEntryPoint.inf

[Components.EBC]
  MdePkg/Library/BaseIoLibIntrinsic/BaseIoLibIntrinsic.inf
  MdePkg/Library/UefiRuntimeLib/UefiRuntimeLib.inf

[Components.ARM, Components.AARCH64]
  MdePkg/Library/BaseIoLibIntrinsic/BaseIoLibIntrinsicArmVirt.inf
  MdePkg/Library/CompilerIntrinsicsLib/CompilerIntrinsicsLib.inf

[Components.RISCV64]
  MdePkg/Library/BaseRiscVSbiLib/BaseRiscVSbiLib.inf
  MdePkg/Library/BaseSerialPortLibRiscVSbiLib/BaseSerialPortLibRiscVSbiLib.inf
  MdePkg/Library/BaseSerialPortLibRiscVSbiLib/BaseSerialPortLibRiscVSbiLibRam.inf

[Components.LOONGARCH64]
  MdePkg/Library/PeiServicesTablePointerLibKs0/PeiServicesTablePointerLibKs0.inf

[BuildOptions]
