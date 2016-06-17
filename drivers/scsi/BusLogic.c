/*

  Linux Driver for BusLogic MultiMaster and FlashPoint SCSI Host Adapters

  Copyright 1995-1998 by Leonard N. Zubkoff <lnz@dandelion.com>

  This program is free software; you may redistribute and/or modify it under
  the terms of the GNU General Public License Version 2 as published by the
  Free Software Foundation.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY, without even the implied warranty of MERCHANTABILITY
  or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
  for complete details.

  The author respectfully requests that any modifications to this software be
  sent directly to him for evaluation and testing.

  Special thanks to Wayne Yen, Jin-Lon Hon, and Alex Win of BusLogic, whose
  advice has been invaluable, to David Gentzel, for writing the original Linux
  BusLogic driver, and to Paul Gortmaker, for being such a dedicated test site.

  Finally, special thanks to Mylex/BusLogic for making the FlashPoint SCCB
  Manager available as freely redistributable source code.

*/


#define BusLogic_DriverVersion		"2.1.15"
#define BusLogic_DriverDate		"17 August 1998"


#include <linux/version.h>
#include <linux/module.h>
#include <linux/config.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/blk.h>
#include <linux/blkdev.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/stat.h>
#include <linux/pci.h>
#include <linux/spinlock.h>
#include <asm/dma.h>
#include <asm/io.h>
#include <asm/system.h>
#include "scsi.h"
#include "hosts.h"
#include "sd.h"
#include "BusLogic.h"
#include "FlashPoint.c"


/*
  BusLogic_DriverOptionsCount is a count of the number of BusLogic Driver
  Options specifications provided via the Linux Kernel Command Line or via
  the Loadable Kernel Module Installation Facility.
*/

static int
  BusLogic_DriverOptionsCount;


/*
  BusLogic_DriverOptions is an array of Driver Options structures representing
  BusLogic Driver Options specifications provided via the Linux Kernel Command
  Line or via the Loadable Kernel Module Installation Facility.
*/

static BusLogic_DriverOptions_T
  BusLogic_DriverOptions[BusLogic_MaxHostAdapters];


/*
  BusLogic can be assigned a string by insmod.
*/

#ifdef MODULE
static char *BusLogic;
MODULE_PARM(BusLogic, "s");
#endif


/*
  BusLogic_ProbeOptions is a set of Probe Options to be applied across
  all BusLogic Host Adapters.
*/

static BusLogic_ProbeOptions_T
  BusLogic_ProbeOptions;


/*
  BusLogic_GlobalOptions is a set of Global Options to be applied across
  all BusLogic Host Adapters.
*/

static BusLogic_GlobalOptions_T
  BusLogic_GlobalOptions;


/*
  BusLogic_FirstRegisteredHostAdapter and BusLogic_LastRegisteredHostAdapter
  are pointers to the first and last registered BusLogic Host Adapters.
*/

static BusLogic_HostAdapter_T
  *BusLogic_FirstRegisteredHostAdapter,
  *BusLogic_LastRegisteredHostAdapter;


/*
  BusLogic_ProbeInfoCount is the number of entries in BusLogic_ProbeInfoList.
*/

static int
  BusLogic_ProbeInfoCount;


/*
  BusLogic_ProbeInfoList is the list of I/O Addresses and Bus Probe Information
  to be checked for potential BusLogic Host Adapters.  It is initialized by
  interrogating the PCI Configuration Space on PCI machines as well as from the
  list of standard BusLogic I/O Addresses.
*/

static BusLogic_ProbeInfo_T
  *BusLogic_ProbeInfoList;


/*
  BusLogic_CommandFailureReason holds a string identifying the reason why a
  call to BusLogic_Command failed.  It is only non-NULL when BusLogic_Command
  returns a failure code.
*/

static char
  *BusLogic_CommandFailureReason;

/*
  BusLogic_AnnounceDriver announces the Driver Version and Date, Author's
  Name, Copyright Notice, and Electronic Mail Address.
*/

static void BusLogic_AnnounceDriver(BusLogic_HostAdapter_T *HostAdapter)
{
  BusLogic_Announce("***** BusLogic SCSI Driver Version "
		    BusLogic_DriverVersion " of "
		    BusLogic_DriverDate " *****\n", HostAdapter);
  BusLogic_Announce("Copyright 1995-1998 by Leonard N. Zubkoff "
		    "<lnz@dandelion.com>\n", HostAdapter);
}


/*
  BusLogic_DriverInfo returns the Host Adapter Name to identify this SCSI
  Driver and Host Adapter.
*/

const char *BusLogic_DriverInfo(SCSI_Host_T *Host)
{
  BusLogic_HostAdapter_T *HostAdapter =
    (BusLogic_HostAdapter_T *) Host->hostdata;
  return HostAdapter->FullModelName;
}


/*
  BusLogic_RegisterHostAdapter adds Host Adapter to the list of registered
  BusLogic Host Adapters.
*/

static void BusLogic_RegisterHostAdapter(BusLogic_HostAdapter_T *HostAdapter)
{
  HostAdapter->Next = NULL;
  if (BusLogic_FirstRegisteredHostAdapter == NULL)
    {
      BusLogic_FirstRegisteredHostAdapter = HostAdapter;
      BusLogic_LastRegisteredHostAdapter = HostAdapter;
    }
  else
    {
      BusLogic_LastRegisteredHostAdapter->Next = HostAdapter;
      BusLogic_LastRegisteredHostAdapter = HostAdapter;
    }
}


/*
  BusLogic_UnregisterHostAdapter removes Host Adapter from the list of
  registered BusLogic Host Adapters.
*/

static void BusLogic_UnregisterHostAdapter(BusLogic_HostAdapter_T *HostAdapter)
{
  if (HostAdapter == BusLogic_FirstRegisteredHostAdapter)
    {
      BusLogic_FirstRegisteredHostAdapter =
	BusLogic_FirstRegisteredHostAdapter->Next;
      if (HostAdapter == BusLogic_LastRegisteredHostAdapter)
	BusLogic_LastRegisteredHostAdapter = NULL;
    }
  else
    {
      BusLogic_HostAdapter_T *PreviousHostAdapter =
	BusLogic_FirstRegisteredHostAdapter;
      while (PreviousHostAdapter != NULL &&
	     PreviousHostAdapter->Next != HostAdapter)
	PreviousHostAdapter = PreviousHostAdapter->Next;
      if (PreviousHostAdapter != NULL)
	PreviousHostAdapter->Next = HostAdapter->Next;
    }
  HostAdapter->Next = NULL;
}


/*
  BusLogic_InitializeCCBs initializes a group of Command Control Blocks (CCBs)
  for Host Adapter from the BlockSize bytes located at BlockPointer.  The newly
  created CCBs are added to Host Adapter's free list.
*/

static void BusLogic_InitializeCCBs(BusLogic_HostAdapter_T *HostAdapter,
				    void *BlockPointer, int BlockSize)
{
  BusLogic_CCB_T *CCB = (BusLogic_CCB_T *) BlockPointer;
  memset(BlockPointer, 0, BlockSize);
  CCB->AllocationGroupHead = true;
  while ((BlockSize -= sizeof(BusLogic_CCB_T)) >= 0)
    {
      CCB->Status = BusLogic_CCB_Free;
      CCB->HostAdapter = HostAdapter;
      if (BusLogic_FlashPointHostAdapterP(HostAdapter))
	{
	  CCB->CallbackFunction = BusLogic_QueueCompletedCCB;
	  CCB->BaseAddress = HostAdapter->FlashPointInfo.BaseAddress;
	}
      CCB->Next = HostAdapter->Free_CCBs;
      CCB->NextAll = HostAdapter->All_CCBs;
      HostAdapter->Free_CCBs = CCB;
      HostAdapter->All_CCBs = CCB;
      HostAdapter->AllocatedCCBs++;
      CCB++;
    }
}


/*
  BusLogic_CreateInitialCCBs allocates the initial CCBs for Host Adapter.
*/

static boolean BusLogic_CreateInitialCCBs(BusLogic_HostAdapter_T *HostAdapter)
{
  int BlockSize = BusLogic_CCB_AllocationGroupSize * sizeof(BusLogic_CCB_T);
  while (HostAdapter->AllocatedCCBs < HostAdapter->InitialCCBs)
    {
      void *BlockPointer = kmalloc(BlockSize,
				   (HostAdapter->BounceBuffersRequired
				    ? GFP_ATOMIC | GFP_DMA
				    : GFP_ATOMIC));
      if (BlockPointer == NULL)
	{
	  BusLogic_Error("UNABLE TO ALLOCATE CCB GROUP - DETACHING\n",
			 HostAdapter);
	  return false;
	}
      BusLogic_InitializeCCBs(HostAdapter, BlockPointer, BlockSize);
    }
  return true;
}


/*
  BusLogic_DestroyCCBs deallocates the CCBs for Host Adapter.
*/

static void BusLogic_DestroyCCBs(BusLogic_HostAdapter_T *HostAdapter)
{
  BusLogic_CCB_T *NextCCB = HostAdapter->All_CCBs, *CCB;
  HostAdapter->All_CCBs = NULL;
  HostAdapter->Free_CCBs = NULL;
  while ((CCB = NextCCB) != NULL)
    {
      NextCCB = CCB->NextAll;
      if (CCB->AllocationGroupHead)
	kfree(CCB);
    }
}


/*
  BusLogic_CreateAdditionalCCBs allocates Additional CCBs for Host Adapter.  If
  allocation fails and there are no remaining CCBs available, the Driver Queue
  Depth is decreased to a known safe value to avoid potential deadlocks when
  multiple host adapters share the same IRQ Channel.
*/

static void BusLogic_CreateAdditionalCCBs(BusLogic_HostAdapter_T *HostAdapter,
					  int AdditionalCCBs,
					  boolean SuccessMessageP)
{
  int BlockSize = BusLogic_CCB_AllocationGroupSize * sizeof(BusLogic_CCB_T);
  int PreviouslyAllocated = HostAdapter->AllocatedCCBs;
  if (AdditionalCCBs <= 0) return;
  while (HostAdapter->AllocatedCCBs - PreviouslyAllocated < AdditionalCCBs)
    {
      void *BlockPointer = kmalloc(BlockSize,
				   (HostAdapter->BounceBuffersRequired
				    ? GFP_ATOMIC | GFP_DMA
				    : GFP_ATOMIC));
      if (BlockPointer == NULL) break;
      BusLogic_InitializeCCBs(HostAdapter, BlockPointer, BlockSize);
    }
  if (HostAdapter->AllocatedCCBs > PreviouslyAllocated)
    {
      if (SuccessMessageP)
	BusLogic_Notice("Allocated %d additional CCBs (total now %d)\n",
			HostAdapter,
			HostAdapter->AllocatedCCBs - PreviouslyAllocated,
			HostAdapter->AllocatedCCBs);
      return;
    }
  BusLogic_Notice("Failed to allocate additional CCBs\n", HostAdapter);
  if (HostAdapter->DriverQueueDepth >
      HostAdapter->AllocatedCCBs - HostAdapter->TargetDeviceCount)
    {
      HostAdapter->DriverQueueDepth =
	HostAdapter->AllocatedCCBs - HostAdapter->TargetDeviceCount;
      HostAdapter->SCSI_Host->can_queue = HostAdapter->DriverQueueDepth;
    }
}


/*
  BusLogic_AllocateCCB allocates a CCB from Host Adapter's free list,
  allocating more memory from the Kernel if necessary.  The Host Adapter's
  Lock should already have been acquired by the caller.
*/

static BusLogic_CCB_T *BusLogic_AllocateCCB(BusLogic_HostAdapter_T
					    *HostAdapter)
{
  static unsigned long SerialNumber = 0;
  BusLogic_CCB_T *CCB;
  CCB = HostAdapter->Free_CCBs;
  if (CCB != NULL)
    {
      CCB->SerialNumber = ++SerialNumber;
      HostAdapter->Free_CCBs = CCB->Next;
      CCB->Next = NULL;
      if (HostAdapter->Free_CCBs == NULL)
	BusLogic_CreateAdditionalCCBs(HostAdapter,
				      HostAdapter->IncrementalCCBs,
				      true);
      return CCB;
    }
  BusLogic_CreateAdditionalCCBs(HostAdapter,
				HostAdapter->IncrementalCCBs,
				true);
  CCB = HostAdapter->Free_CCBs;
  if (CCB == NULL) return NULL;
  CCB->SerialNumber = ++SerialNumber;
  HostAdapter->Free_CCBs = CCB->Next;
  CCB->Next = NULL;
  return CCB;
}


/*
  BusLogic_DeallocateCCB deallocates a CCB, returning it to the Host Adapter's
  free list.  The Host Adapter's Lock should already have been acquired by the
  caller.
*/

static void BusLogic_DeallocateCCB(BusLogic_CCB_T *CCB)
{
  BusLogic_HostAdapter_T *HostAdapter = CCB->HostAdapter;
  CCB->Command = NULL;
  CCB->Status = BusLogic_CCB_Free;
  CCB->Next = HostAdapter->Free_CCBs;
  HostAdapter->Free_CCBs = CCB;
}


/*
  BusLogic_Command sends the command OperationCode to HostAdapter, optionally
  providing ParameterLength bytes of ParameterData and receiving at most
  ReplyLength bytes of ReplyData; any excess reply data is received but
  discarded.

  On success, this function returns the number of reply bytes read from
  the Host Adapter (including any discarded data); on failure, it returns
  -1 if the command was invalid, or -2 if a timeout occurred.

  BusLogic_Command is called exclusively during host adapter detection and
  initialization, so performance and latency are not critical, and exclusive
  access to the Host Adapter hardware is assumed.  Once the host adapter and
  driver are initialized, the only Host Adapter command that is issued is the
  single byte Execute Mailbox Command operation code, which does not require
  waiting for the Host Adapter Ready bit to be set in the Status Register.
*/

static int BusLogic_Command(BusLogic_HostAdapter_T *HostAdapter,
			    BusLogic_OperationCode_T OperationCode,
			    void *ParameterData,
			    int ParameterLength,
			    void *ReplyData,
			    int ReplyLength)
{
  unsigned char *ParameterPointer = (unsigned char *) ParameterData;
  unsigned char *ReplyPointer = (unsigned char *) ReplyData;
  BusLogic_StatusRegister_T StatusRegister;
  BusLogic_InterruptRegister_T InterruptRegister;
  ProcessorFlags_T ProcessorFlags = 0;
  int ReplyBytes = 0, Result;
  long TimeoutCounter;
  /*
    Clear out the Reply Data if provided.
  */
  if (ReplyLength > 0)
    memset(ReplyData, 0, ReplyLength);
  /*
    If the IRQ Channel has not yet been acquired, then interrupts must be
    disabled while issuing host adapter commands since a Command Complete
    interrupt could occur if the IRQ Channel was previously enabled by another
    BusLogic Host Adapter or another driver sharing the same IRQ Channel.
  */
  if (!HostAdapter->IRQ_ChannelAcquired)
    {
      save_flags(ProcessorFlags);
      cli();
    }
  /*
    Wait for the Host Adapter Ready bit to be set and the Command/Parameter
    Register Busy bit to be reset in the Status Register.
  */
  TimeoutCounter = 10000;
  while (--TimeoutCounter >= 0)
    {
      StatusRegister.All = BusLogic_ReadStatusRegister(HostAdapter);
      if (StatusRegister.Bits.HostAdapterReady &&
	  !StatusRegister.Bits.CommandParameterRegisterBusy)
	break;
      udelay(100);
    }
  if (TimeoutCounter < 0)
    {
      BusLogic_CommandFailureReason = "Timeout waiting for Host Adapter Ready";
      Result = -2;
      goto Done;
    }
  /*
    Write the OperationCode to the Command/Parameter Register.
  */
  HostAdapter->HostAdapterCommandCompleted = false;
  BusLogic_WriteCommandParameterRegister(HostAdapter, OperationCode);
  /*
    Write any additional Parameter Bytes.
  */
  TimeoutCounter = 10000;
  while (ParameterLength > 0 && --TimeoutCounter >= 0)
    {
      /*
	Wait 100 microseconds to give the Host Adapter enough time to determine
	whether the last value written to the Command/Parameter Register was
	valid or not.  If the Command Complete bit is set in the Interrupt
	Register, then the Command Invalid bit in the Status Register will be
	reset if the Operation Code or Parameter was valid and the command
	has completed, or set if the Operation Code or Parameter was invalid.
	If the Data In Register Ready bit is set in the Status Register, then
	the Operation Code was valid, and data is waiting to be read back
	from the Host Adapter.  Otherwise, wait for the Command/Parameter
	Register Busy bit in the Status Register to be reset.
      */
      udelay(100);
      InterruptRegister.All = BusLogic_ReadInterruptRegister(HostAdapter);
      StatusRegister.All = BusLogic_ReadStatusRegister(HostAdapter);
      if (InterruptRegister.Bits.CommandComplete) break;
      if (HostAdapter->HostAdapterCommandCompleted) break;
      if (StatusRegister.Bits.DataInRegisterReady) break;
      if (StatusRegister.Bits.CommandParameterRegisterBusy) continue;
      BusLogic_WriteCommandParameterRegister(HostAdapter, *ParameterPointer++);
      ParameterLength--;
    }
  if (TimeoutCounter < 0)
    {
      BusLogic_CommandFailureReason =
	"Timeout waiting for Parameter Acceptance";
      Result = -2;
      goto Done;
    }
  /*
    The Modify I/O Address command does not cause a Command Complete Interrupt.
  */
  if (OperationCode == BusLogic_ModifyIOAddress)
    {
      StatusRegister.All = BusLogic_ReadStatusRegister(HostAdapter);
      if (StatusRegister.Bits.CommandInvalid)
	{
	  BusLogic_CommandFailureReason = "Modify I/O Address Invalid";
	  Result = -1;
	  goto Done;
	}
      if (BusLogic_GlobalOptions.TraceConfiguration)
	BusLogic_Notice("BusLogic_Command(%02X) Status = %02X: "
			"(Modify I/O Address)\n", HostAdapter,
			OperationCode, StatusRegister.All);
      Result = 0;
      goto Done;
    }
  /*
    Select an appropriate timeout value for awaiting command completion.
  */
  switch (OperationCode)
    {
    case BusLogic_InquireInstalledDevicesID0to7:
    case BusLogic_InquireInstalledDevicesID8to15:
    case BusLogic_InquireTargetDevices:
      /* Approximately 60 seconds. */
      TimeoutCounter = 60*10000;
      break;
    default:
      /* Approximately 1 second. */
      TimeoutCounter = 10000;
      break;
    }
  /*
    Receive any Reply Bytes, waiting for either the Command Complete bit to
    be set in the Interrupt Register, or for the Interrupt Handler to set the
    Host Adapter Command Completed bit in the Host Adapter structure.
  */
  while (--TimeoutCounter >= 0)
    {
      InterruptRegister.All = BusLogic_ReadInterruptRegister(HostAdapter);
      StatusRegister.All = BusLogic_ReadStatusRegister(HostAdapter);
      if (InterruptRegister.Bits.CommandComplete) break;
      if (HostAdapter->HostAdapterCommandCompleted) break;
      if (StatusRegister.Bits.DataInRegisterReady)
	{
	  if (++ReplyBytes <= ReplyLength)
	    *ReplyPointer++ = BusLogic_ReadDataInRegister(HostAdapter);
	  else BusLogic_ReadDataInRegister(HostAdapter);
	}
      if (OperationCode == BusLogic_FetchHostAdapterLocalRAM &&
	  StatusRegister.Bits.HostAdapterReady) break;
      udelay(100);
    }
  if (TimeoutCounter < 0)
    {
      BusLogic_CommandFailureReason = "Timeout waiting for Command Complete";
      Result = -2;
      goto Done;
    }
  /*
    Clear any pending Command Complete Interrupt.
  */
  BusLogic_InterruptReset(HostAdapter);
  /*
    Provide tracing information if requested.
  */
  if (BusLogic_GlobalOptions.TraceConfiguration)
    {
      int i;
      BusLogic_Notice("BusLogic_Command(%02X) Status = %02X: %2d ==> %2d:",
		      HostAdapter, OperationCode,
		      StatusRegister.All, ReplyLength, ReplyBytes);
      if (ReplyLength > ReplyBytes) ReplyLength = ReplyBytes;
      for (i = 0; i < ReplyLength; i++)
	BusLogic_Notice(" %02X", HostAdapter,
			((unsigned char *) ReplyData)[i]);
      BusLogic_Notice("\n", HostAdapter);
    }
  /*
    Process Command Invalid conditions.
  */
  if (StatusRegister.Bits.CommandInvalid)
    {
      /*
	Some early BusLogic Host Adapters may not recover properly from
	a Command Invalid condition, so if this appears to be the case,
	a Soft Reset is issued to the Host Adapter.  Potentially invalid
	commands are never attempted after Mailbox Initialization is
	performed, so there should be no Host Adapter state lost by a
	Soft Reset in response to a Command Invalid condition.
      */
      udelay(1000);
      StatusRegister.All = BusLogic_ReadStatusRegister(HostAdapter);
      if (StatusRegister.Bits.CommandInvalid ||
	  StatusRegister.Bits.Reserved ||
	  StatusRegister.Bits.DataInRegisterReady ||
	  StatusRegister.Bits.CommandParameterRegisterBusy ||
	  !StatusRegister.Bits.HostAdapterReady ||
	  !StatusRegister.Bits.InitializationRequired ||
	  StatusRegister.Bits.DiagnosticActive ||
	  StatusRegister.Bits.DiagnosticFailure)
	{
	  BusLogic_SoftReset(HostAdapter);
	  udelay(1000);
	}
      BusLogic_CommandFailureReason = "Command Invalid";
      Result = -1;
      goto Done;
    }
  /*
    Handle Excess Parameters Supplied conditions.
  */
  if (ParameterLength > 0)
    {
      BusLogic_CommandFailureReason = "Excess Parameters Supplied";
      Result = -1;
      goto Done;
    }
  /*
    Indicate the command completed successfully.
  */
  BusLogic_CommandFailureReason = NULL;
  Result = ReplyBytes;
  /*
    Restore the interrupt status if necessary and return.
  */
Done:
  if (!HostAdapter->IRQ_ChannelAcquired)
    restore_flags(ProcessorFlags);
  return Result;
}


/*
  BusLogic_AppendProbeAddressISA appends a single ISA I/O Address to the list
  of I/O Address and Bus Probe Information to be checked for potential BusLogic
  Host Adapters.
*/

static void BusLogic_AppendProbeAddressISA(BusLogic_IO_Address_T IO_Address)
{
  BusLogic_ProbeInfo_T *ProbeInfo;
  if (BusLogic_ProbeInfoCount >= BusLogic_MaxHostAdapters) return;
  ProbeInfo = &BusLogic_ProbeInfoList[BusLogic_ProbeInfoCount++];
  ProbeInfo->HostAdapterType = BusLogic_MultiMaster;
  ProbeInfo->HostAdapterBusType = BusLogic_ISA_Bus;
  ProbeInfo->IO_Address = IO_Address;
}


/*
  BusLogic_InitializeProbeInfoListISA initializes the list of I/O Address and
  Bus Probe Information to be checked for potential BusLogic SCSI Host Adapters
  only from the list of standard BusLogic MultiMaster ISA I/O Addresses.
*/

static void BusLogic_InitializeProbeInfoListISA(BusLogic_HostAdapter_T
						*PrototypeHostAdapter)
{
  /*
    If BusLogic Driver Options specifications requested that ISA Bus Probes
    be inhibited, do not proceed further.
  */
  if (BusLogic_ProbeOptions.NoProbeISA) return;
  /*
    Append the list of standard BusLogic MultiMaster ISA I/O Addresses.
  */
  if (BusLogic_ProbeOptions.LimitedProbeISA
      ? BusLogic_ProbeOptions.Probe330
      : check_region(0x330, BusLogic_MultiMasterAddressCount) == 0)
    BusLogic_AppendProbeAddressISA(0x330);
  if (BusLogic_ProbeOptions.LimitedProbeISA
      ? BusLogic_ProbeOptions.Probe334
      : check_region(0x334, BusLogic_MultiMasterAddressCount) == 0)
    BusLogic_AppendProbeAddressISA(0x334);
  if (BusLogic_ProbeOptions.LimitedProbeISA
      ? BusLogic_ProbeOptions.Probe230
      : check_region(0x230, BusLogic_MultiMasterAddressCount) == 0)
    BusLogic_AppendProbeAddressISA(0x230);
  if (BusLogic_ProbeOptions.LimitedProbeISA
      ? BusLogic_ProbeOptions.Probe234
      : check_region(0x234, BusLogic_MultiMasterAddressCount) == 0)
    BusLogic_AppendProbeAddressISA(0x234);
  if (BusLogic_ProbeOptions.LimitedProbeISA
      ? BusLogic_ProbeOptions.Probe130
      : check_region(0x130, BusLogic_MultiMasterAddressCount) == 0)
    BusLogic_AppendProbeAddressISA(0x130);
  if (BusLogic_ProbeOptions.LimitedProbeISA
      ? BusLogic_ProbeOptions.Probe134
      : check_region(0x134, BusLogic_MultiMasterAddressCount) == 0)
    BusLogic_AppendProbeAddressISA(0x134);
}


#ifdef CONFIG_PCI


/*
  BusLogic_SortProbeInfo sorts a section of BusLogic_ProbeInfoList in order
  of increasing PCI Bus and Device Number.
*/

static void BusLogic_SortProbeInfo(BusLogic_ProbeInfo_T *ProbeInfoList,
				   int ProbeInfoCount)
{
  int LastInterchange = ProbeInfoCount-1, Bound, j;
  while (LastInterchange > 0)
    {
      Bound = LastInterchange;
      LastInterchange = 0;
      for (j = 0; j < Bound; j++)
	{
	  BusLogic_ProbeInfo_T *ProbeInfo1 = &ProbeInfoList[j];
	  BusLogic_ProbeInfo_T *ProbeInfo2 = &ProbeInfoList[j+1];
	  if (ProbeInfo1->Bus > ProbeInfo2->Bus ||
	      (ProbeInfo1->Bus == ProbeInfo2->Bus &&
	       (ProbeInfo1->Device > ProbeInfo2->Device)))
	    {
	      BusLogic_ProbeInfo_T TempProbeInfo;
	      memcpy(&TempProbeInfo, ProbeInfo1, sizeof(BusLogic_ProbeInfo_T));
	      memcpy(ProbeInfo1, ProbeInfo2, sizeof(BusLogic_ProbeInfo_T));
	      memcpy(ProbeInfo2, &TempProbeInfo, sizeof(BusLogic_ProbeInfo_T));
	      LastInterchange = j;
	    }
	}
    }
}


/*
  BusLogic_InitializeMultiMasterProbeInfo initializes the list of I/O Address
  and Bus Probe Information to be checked for potential BusLogic MultiMaster
  SCSI Host Adapters by interrogating the PCI Configuration Space on PCI
  machines as well as from the list of standard BusLogic MultiMaster ISA
  I/O Addresses.  It returns the number of PCI MultiMaster Host Adapters found.
*/

static int BusLogic_InitializeMultiMasterProbeInfo(BusLogic_HostAdapter_T
						   *PrototypeHostAdapter)
{
  BusLogic_ProbeInfo_T *PrimaryProbeInfo =
    &BusLogic_ProbeInfoList[BusLogic_ProbeInfoCount];
  int NonPrimaryPCIMultiMasterIndex = BusLogic_ProbeInfoCount + 1;
  int NonPrimaryPCIMultiMasterCount = 0, PCIMultiMasterCount = 0;
  boolean ForceBusDeviceScanningOrder = false;
  boolean ForceBusDeviceScanningOrderChecked = false;
  boolean StandardAddressSeen[6];
  PCI_Device_T *PCI_Device = NULL;
  int i;
  if (BusLogic_ProbeInfoCount >= BusLogic_MaxHostAdapters) return 0;
  BusLogic_ProbeInfoCount++;
  for (i = 0; i < 6; i++)
    StandardAddressSeen[i] = false;
  /*
    Iterate over the MultiMaster PCI Host Adapters.  For each enumerated host
    adapter, determine whether its ISA Compatible I/O Port is enabled and if
    so, whether it is assigned the Primary I/O Address.  A host adapter that is
    assigned the Primary I/O Address will always be the preferred boot device.
    The MultiMaster BIOS will first recognize a host adapter at the Primary I/O
    Address, then any other PCI host adapters, and finally any host adapters
    located at the remaining standard ISA I/O Addresses.  When a PCI host
    adapter is found with its ISA Compatible I/O Port enabled, a command is
    issued to disable the ISA Compatible I/O Port, and it is noted that the
    particular standard ISA I/O Address need not be probed.
  */
  PrimaryProbeInfo->IO_Address = 0;
  while ((PCI_Device = pci_find_device(PCI_VENDOR_ID_BUSLOGIC,
				       PCI_DEVICE_ID_BUSLOGIC_MULTIMASTER,
				       PCI_Device)) != NULL)
    {
      BusLogic_HostAdapter_T *HostAdapter = PrototypeHostAdapter;
      BusLogic_PCIHostAdapterInformation_T PCIHostAdapterInformation;
      BusLogic_ModifyIOAddressRequest_T ModifyIOAddressRequest;
      unsigned char Bus = PCI_Device->bus->number;
      unsigned char Device = PCI_Device->devfn >> 3;
      unsigned int IRQ_Channel;
      unsigned long BaseAddress0;
      unsigned long BaseAddress1;
      BusLogic_IO_Address_T IO_Address;
      BusLogic_PCI_Address_T PCI_Address;

      if (pci_enable_device(PCI_Device))
      	continue;
      
      IRQ_Channel = PCI_Device->irq;
      IO_Address  = BaseAddress0 = pci_resource_start(PCI_Device, 0);
      PCI_Address = BaseAddress1 = pci_resource_start(PCI_Device, 1);

      if (pci_resource_flags(PCI_Device, 0) & IORESOURCE_MEM)
	{
	  BusLogic_Error("BusLogic: Base Address0 0x%X not I/O for "
			 "MultiMaster Host Adapter\n", NULL, BaseAddress0);
	  BusLogic_Error("at PCI Bus %d Device %d I/O Address 0x%X\n",
			 NULL, Bus, Device, IO_Address);
	  continue;
	}
      if (pci_resource_flags(PCI_Device,1) & IORESOURCE_IO)
	{
	  BusLogic_Error("BusLogic: Base Address1 0x%X not Memory for "
			 "MultiMaster Host Adapter\n", NULL, BaseAddress1);
	  BusLogic_Error("at PCI Bus %d Device %d PCI Address 0x%X\n",
			 NULL, Bus, Device, PCI_Address);
	  continue;
	}
      if (IRQ_Channel == 0)
	{
	  BusLogic_Error("BusLogic: IRQ Channel %d illegal for "
			 "MultiMaster Host Adapter\n", NULL, IRQ_Channel);
	  BusLogic_Error("at PCI Bus %d Device %d I/O Address 0x%X\n",
			 NULL, Bus, Device, IO_Address);
	  continue;
	}
      if (BusLogic_GlobalOptions.TraceProbe)
	{
	  BusLogic_Notice("BusLogic: PCI MultiMaster Host Adapter "
			  "detected at\n", NULL);
	  BusLogic_Notice("BusLogic: PCI Bus %d Device %d I/O Address "
			  "0x%X PCI Address 0x%X\n", NULL,
			  Bus, Device, IO_Address, PCI_Address);
	}
      /*
	Issue the Inquire PCI Host Adapter Information command to determine
	the ISA Compatible I/O Port.  If the ISA Compatible I/O Port is
	known and enabled, note that the particular Standard ISA I/O
	Address should not be probed.
      */
      HostAdapter->IO_Address = IO_Address;
      BusLogic_InterruptReset(HostAdapter);
      if (BusLogic_Command(HostAdapter,
			   BusLogic_InquirePCIHostAdapterInformation,
			   NULL, 0, &PCIHostAdapterInformation,
			   sizeof(PCIHostAdapterInformation))
	  == sizeof(PCIHostAdapterInformation))
	{
	  if (PCIHostAdapterInformation.ISACompatibleIOPort < 6)
	    StandardAddressSeen[PCIHostAdapterInformation
				.ISACompatibleIOPort] = true;
	}
      else PCIHostAdapterInformation.ISACompatibleIOPort =
	     BusLogic_IO_Disable;
      /*
	Issue the Modify I/O Address command to disable the ISA Compatible
	I/O Port.
      */
      ModifyIOAddressRequest = BusLogic_IO_Disable;
      BusLogic_Command(HostAdapter, BusLogic_ModifyIOAddress,
		       &ModifyIOAddressRequest,
		       sizeof(ModifyIOAddressRequest), NULL, 0);
      /*
	For the first MultiMaster Host Adapter enumerated, issue the Fetch
	Host Adapter Local RAM command to read byte 45 of the AutoSCSI area,
	for the setting of the "Use Bus And Device # For PCI Scanning Seq."
	option.  Issue the Inquire Board ID command since this option is
	only valid for the BT-948/958/958D.
      */
      if (!ForceBusDeviceScanningOrderChecked)
	{
	  BusLogic_FetchHostAdapterLocalRAMRequest_T
	    FetchHostAdapterLocalRAMRequest;
	  BusLogic_AutoSCSIByte45_T AutoSCSIByte45;
	  BusLogic_BoardID_T BoardID;
	  FetchHostAdapterLocalRAMRequest.ByteOffset =
	    BusLogic_AutoSCSI_BaseOffset + 45;
	  FetchHostAdapterLocalRAMRequest.ByteCount =
	    sizeof(AutoSCSIByte45);
	  BusLogic_Command(HostAdapter,
			   BusLogic_FetchHostAdapterLocalRAM,
			   &FetchHostAdapterLocalRAMRequest,
			   sizeof(FetchHostAdapterLocalRAMRequest),
			   &AutoSCSIByte45, sizeof(AutoSCSIByte45));
	  BusLogic_Command(HostAdapter, BusLogic_InquireBoardID,
			   NULL, 0, &BoardID, sizeof(BoardID));
	  if (BoardID.FirmwareVersion1stDigit == '5')
	    ForceBusDeviceScanningOrder =
	      AutoSCSIByte45.ForceBusDeviceScanningOrder;
	  ForceBusDeviceScanningOrderChecked = true;
	}
      /*
	Determine whether this MultiMaster Host Adapter has its ISA
	Compatible I/O Port enabled and is assigned the Primary I/O Address.
	If it does, then it is the Primary MultiMaster Host Adapter and must
	be recognized first.  If it does not, then it is added to the list
	for probing after any Primary MultiMaster Host Adapter is probed.
      */
      if (PCIHostAdapterInformation.ISACompatibleIOPort == BusLogic_IO_330)
	{
	  PrimaryProbeInfo->HostAdapterType = BusLogic_MultiMaster;
	  PrimaryProbeInfo->HostAdapterBusType = BusLogic_PCI_Bus;
	  PrimaryProbeInfo->IO_Address = IO_Address;
	  PrimaryProbeInfo->PCI_Address = PCI_Address;
	  PrimaryProbeInfo->Bus = Bus;
	  PrimaryProbeInfo->Device = Device;
	  PrimaryProbeInfo->IRQ_Channel = IRQ_Channel;
	  PCIMultiMasterCount++;
	}
      else if (BusLogic_ProbeInfoCount < BusLogic_MaxHostAdapters)
	{
	  BusLogic_ProbeInfo_T *ProbeInfo =
	    &BusLogic_ProbeInfoList[BusLogic_ProbeInfoCount++];
	  ProbeInfo->HostAdapterType = BusLogic_MultiMaster;
	  ProbeInfo->HostAdapterBusType = BusLogic_PCI_Bus;
	  ProbeInfo->IO_Address = IO_Address;
	  ProbeInfo->PCI_Address = PCI_Address;
	  ProbeInfo->Bus = Bus;
	  ProbeInfo->Device = Device;
	  ProbeInfo->IRQ_Channel = IRQ_Channel;
	  NonPrimaryPCIMultiMasterCount++;
	  PCIMultiMasterCount++;
	}
      else BusLogic_Warning("BusLogic: Too many Host Adapters "
			    "detected\n", NULL);
    }
  /*
    If the AutoSCSI "Use Bus And Device # For PCI Scanning Seq." option is ON
    for the first enumerated MultiMaster Host Adapter, and if that host adapter
    is a BT-948/958/958D, then the MultiMaster BIOS will recognize MultiMaster
    Host Adapters in the order of increasing PCI Bus and Device Number.  In
    that case, sort the probe information into the same order the BIOS uses.
    If this option is OFF, then the MultiMaster BIOS will recognize MultiMaster
    Host Adapters in the order they are enumerated by the PCI BIOS, and hence
    no sorting is necessary.
  */
  if (ForceBusDeviceScanningOrder)
    BusLogic_SortProbeInfo(&BusLogic_ProbeInfoList[
			      NonPrimaryPCIMultiMasterIndex],
			   NonPrimaryPCIMultiMasterCount);
  /*
    If no PCI MultiMaster Host Adapter is assigned the Primary I/O Address,
    then the Primary I/O Address must be probed explicitly before any PCI
    host adapters are probed.
  */
  if (!BusLogic_ProbeOptions.NoProbeISA)
    if (PrimaryProbeInfo->IO_Address == 0 &&
	(BusLogic_ProbeOptions.LimitedProbeISA
	 ? BusLogic_ProbeOptions.Probe330
	 : check_region(0x330, BusLogic_MultiMasterAddressCount) == 0))
      {
	PrimaryProbeInfo->HostAdapterType = BusLogic_MultiMaster;
	PrimaryProbeInfo->HostAdapterBusType = BusLogic_ISA_Bus;
	PrimaryProbeInfo->IO_Address = 0x330;
      }
  /*
    Append the list of standard BusLogic MultiMaster ISA I/O Addresses,
    omitting the Primary I/O Address which has already been handled.
  */
  if (!BusLogic_ProbeOptions.NoProbeISA)
    {
      if (!StandardAddressSeen[1] &&
	  (BusLogic_ProbeOptions.LimitedProbeISA
	   ? BusLogic_ProbeOptions.Probe334
	   : check_region(0x334, BusLogic_MultiMasterAddressCount) == 0))
	BusLogic_AppendProbeAddressISA(0x334);
      if (!StandardAddressSeen[2] &&
	  (BusLogic_ProbeOptions.LimitedProbeISA
	   ? BusLogic_ProbeOptions.Probe230
	   : check_region(0x230, BusLogic_MultiMasterAddressCount) == 0))
	BusLogic_AppendProbeAddressISA(0x230);
      if (!StandardAddressSeen[3] &&
	  (BusLogic_ProbeOptions.LimitedProbeISA
	   ? BusLogic_ProbeOptions.Probe234
	   : check_region(0x234, BusLogic_MultiMasterAddressCount) == 0))
	BusLogic_AppendProbeAddressISA(0x234);
      if (!StandardAddressSeen[4] &&
	  (BusLogic_ProbeOptions.LimitedProbeISA
	   ? BusLogic_ProbeOptions.Probe130
	   : check_region(0x130, BusLogic_MultiMasterAddressCount) == 0))
	BusLogic_AppendProbeAddressISA(0x130);
      if (!StandardAddressSeen[5] &&
	  (BusLogic_ProbeOptions.LimitedProbeISA
	   ? BusLogic_ProbeOptions.Probe134
	   : check_region(0x134, BusLogic_MultiMasterAddressCount) == 0))
	BusLogic_AppendProbeAddressISA(0x134);
    }
  /*
    Iterate over the older non-compliant MultiMaster PCI Host Adapters,
    noting the PCI bus location and assigned IRQ Channel.
  */
  PCI_Device = NULL;
  while ((PCI_Device = pci_find_device(PCI_VENDOR_ID_BUSLOGIC,
				       PCI_DEVICE_ID_BUSLOGIC_MULTIMASTER_NC,
				       PCI_Device)) != NULL)
    {
      unsigned char Bus = PCI_Device->bus->number;
      unsigned char Device = PCI_Device->devfn >> 3;
      unsigned int IRQ_Channel = PCI_Device->irq;
      BusLogic_IO_Address_T IO_Address = pci_resource_start(PCI_Device, 0);

      if (pci_enable_device(PCI_Device))
		continue;

      if (IO_Address == 0 || IRQ_Channel == 0) continue;
      for (i = 0; i < BusLogic_ProbeInfoCount; i++)
	{
	  BusLogic_ProbeInfo_T *ProbeInfo = &BusLogic_ProbeInfoList[i];
	  if (ProbeInfo->IO_Address == IO_Address &&
	      ProbeInfo->HostAdapterType == BusLogic_MultiMaster)
	    {
	      ProbeInfo->HostAdapterBusType = BusLogic_PCI_Bus;
	      ProbeInfo->PCI_Address = 0;
	      ProbeInfo->Bus = Bus;
	      ProbeInfo->Device = Device;
	      ProbeInfo->IRQ_Channel = IRQ_Channel;
	      break;
	    }
	}
    }
  return PCIMultiMasterCount;
}


/*
  BusLogic_InitializeFlashPointProbeInfo initializes the list of I/O Address
  and Bus Probe Information to be checked for potential BusLogic FlashPoint
  Host Adapters by interrogating the PCI Configuration Space.  It returns the
  number of FlashPoint Host Adapters found.
*/

static int BusLogic_InitializeFlashPointProbeInfo(BusLogic_HostAdapter_T
						  *PrototypeHostAdapter)
{
  int FlashPointIndex = BusLogic_ProbeInfoCount, FlashPointCount = 0;
  PCI_Device_T *PCI_Device = NULL;
  /*
    Interrogate PCI Configuration Space for any FlashPoint Host Adapters.
  */
  while ((PCI_Device = pci_find_device(PCI_VENDOR_ID_BUSLOGIC,
				       PCI_DEVICE_ID_BUSLOGIC_FLASHPOINT,
				       PCI_Device)) != NULL)
    {
      unsigned char Bus = PCI_Device->bus->number;
      unsigned char Device = PCI_Device->devfn >> 3;
      unsigned int IRQ_Channel = PCI_Device->irq;
      unsigned long BaseAddress0 = pci_resource_start(PCI_Device, 0);
      unsigned long BaseAddress1 = pci_resource_start(PCI_Device, 1);
      BusLogic_IO_Address_T IO_Address = BaseAddress0;
      BusLogic_PCI_Address_T PCI_Address = BaseAddress1;

      if (pci_enable_device(PCI_Device))
		continue;

#ifndef CONFIG_SCSI_OMIT_FLASHPOINT
      if (pci_resource_flags(PCI_Device, 0) & IORESOURCE_MEM)
	{
	  BusLogic_Error("BusLogic: Base Address0 0x%X not I/O for "
			 "FlashPoint Host Adapter\n", NULL, BaseAddress0);
	  BusLogic_Error("at PCI Bus %d Device %d I/O Address 0x%X\n",
			 NULL, Bus, Device, IO_Address);
	  continue;
	}
      if (pci_resource_flags(PCI_Device, 1) & IORESOURCE_IO)
	{
	  BusLogic_Error("BusLogic: Base Address1 0x%X not Memory for "
			 "FlashPoint Host Adapter\n", NULL, BaseAddress1);
	  BusLogic_Error("at PCI Bus %d Device %d PCI Address 0x%X\n",
			 NULL, Bus, Device, PCI_Address);
	  continue;
	}
      if (IRQ_Channel == 0)
	{
	  BusLogic_Error("BusLogic: IRQ Channel %d illegal for "
			 "FlashPoint Host Adapter\n", NULL, IRQ_Channel);
	  BusLogic_Error("at PCI Bus %d Device %d I/O Address 0x%X\n",
			 NULL, Bus, Device, IO_Address);
	  continue;
	}
      if (BusLogic_GlobalOptions.TraceProbe)
	{
	  BusLogic_Notice("BusLogic: FlashPoint Host Adapter "
			  "detected at\n", NULL);
	  BusLogic_Notice("BusLogic: PCI Bus %d Device %d I/O Address "
			  "0x%X PCI Address 0x%X\n", NULL,
			  Bus, Device, IO_Address, PCI_Address);
	}
      if (BusLogic_ProbeInfoCount < BusLogic_MaxHostAdapters)
	{
	  BusLogic_ProbeInfo_T *ProbeInfo =
	    &BusLogic_ProbeInfoList[BusLogic_ProbeInfoCount++];
	  ProbeInfo->HostAdapterType = BusLogic_FlashPoint;
	  ProbeInfo->HostAdapterBusType = BusLogic_PCI_Bus;
	  ProbeInfo->IO_Address = IO_Address;
	  ProbeInfo->PCI_Address = PCI_Address;
	  ProbeInfo->Bus = Bus;
	  ProbeInfo->Device = Device;
	  ProbeInfo->IRQ_Channel = IRQ_Channel;
	  FlashPointCount++;
	}
      else BusLogic_Warning("BusLogic: Too many Host Adapters "
			    "detected\n", NULL);
#else
      BusLogic_Error("BusLogic: FlashPoint Host Adapter detected at "
		     "PCI Bus %d Device %d\n", NULL, Bus, Device);
      BusLogic_Error("BusLogic: I/O Address 0x%X PCI Address 0x%X, irq %d, "
		     "but FlashPoint\n", NULL, IO_Address, PCI_Address, IRQ_Channel);
      BusLogic_Error("BusLogic: support was omitted in this kernel "
		     "configuration.\n", NULL);
#endif
    }
  /*
    The FlashPoint BIOS will scan for FlashPoint Host Adapters in the order of
    increasing PCI Bus and Device Number, so sort the probe information into
    the same order the BIOS uses.
  */
  BusLogic_SortProbeInfo(&BusLogic_ProbeInfoList[FlashPointIndex],
			 FlashPointCount);
  return FlashPointCount;
}


/*
  BusLogic_InitializeProbeInfoList initializes the list of I/O Address and Bus
  Probe Information to be checked for potential BusLogic SCSI Host Adapters by
  interrogating the PCI Configuration Space on PCI machines as well as from the
  list of standard BusLogic MultiMaster ISA I/O Addresses.  By default, if both
  FlashPoint and PCI MultiMaster Host Adapters are present, this driver will
  probe for FlashPoint Host Adapters first unless the BIOS primary disk is
  controlled by the first PCI MultiMaster Host Adapter, in which case
  MultiMaster Host Adapters will be probed first.  The BusLogic Driver Options
  specifications "MultiMasterFirst" and "FlashPointFirst" can be used to force
  a particular probe order.
*/

static void BusLogic_InitializeProbeInfoList(BusLogic_HostAdapter_T
					     *PrototypeHostAdapter)
{
  /*
    If a PCI BIOS is present, interrogate it for MultiMaster and FlashPoint
    Host Adapters; otherwise, default to the standard ISA MultiMaster probe.
  */
  if (!BusLogic_ProbeOptions.NoProbePCI && pci_present())
    {
      if (BusLogic_ProbeOptions.MultiMasterFirst)
	{
	  BusLogic_InitializeMultiMasterProbeInfo(PrototypeHostAdapter);
	  BusLogic_InitializeFlashPointProbeInfo(PrototypeHostAdapter);
	}
      else if (BusLogic_ProbeOptions.FlashPointFirst)
	{
	  BusLogic_InitializeFlashPointProbeInfo(PrototypeHostAdapter);
	  BusLogic_InitializeMultiMasterProbeInfo(PrototypeHostAdapter);
	}
      else
	{
	  int FlashPointCount =
	    BusLogic_InitializeFlashPointProbeInfo(PrototypeHostAdapter);
	  int PCIMultiMasterCount =
	    BusLogic_InitializeMultiMasterProbeInfo(PrototypeHostAdapter);
	  if (FlashPointCount > 0 && PCIMultiMasterCount > 0)
	    {
	      BusLogic_ProbeInfo_T *ProbeInfo =
		&BusLogic_ProbeInfoList[FlashPointCount];
	      BusLogic_HostAdapter_T *HostAdapter = PrototypeHostAdapter;
	      BusLogic_FetchHostAdapterLocalRAMRequest_T
		FetchHostAdapterLocalRAMRequest;
	      BusLogic_BIOSDriveMapByte_T Drive0MapByte;
	      while (ProbeInfo->HostAdapterBusType != BusLogic_PCI_Bus)
		ProbeInfo++;
	      HostAdapter->IO_Address = ProbeInfo->IO_Address;
	      FetchHostAdapterLocalRAMRequest.ByteOffset =
		BusLogic_BIOS_BaseOffset + BusLogic_BIOS_DriveMapOffset + 0;
	      FetchHostAdapterLocalRAMRequest.ByteCount =
		sizeof(Drive0MapByte);
	      BusLogic_Command(HostAdapter,
			       BusLogic_FetchHostAdapterLocalRAM,
			       &FetchHostAdapterLocalRAMRequest,
			       sizeof(FetchHostAdapterLocalRAMRequest),
			       &Drive0MapByte, sizeof(Drive0MapByte));
	      /*
		If the Map Byte for BIOS Drive 0 indicates that BIOS Drive 0
		is controlled by this PCI MultiMaster Host Adapter, then
		reverse the probe order so that MultiMaster Host Adapters are
		probed before FlashPoint Host Adapters.
	      */
	      if (Drive0MapByte.DiskGeometry !=
		  BusLogic_BIOS_Disk_Not_Installed)
		{
		  BusLogic_ProbeInfo_T
		    SavedProbeInfo[BusLogic_MaxHostAdapters];
		  int MultiMasterCount =
		    BusLogic_ProbeInfoCount - FlashPointCount;
		  memcpy(SavedProbeInfo,
			 BusLogic_ProbeInfoList,
			 BusLogic_ProbeInfoCount
			 * sizeof(BusLogic_ProbeInfo_T));
		  memcpy(&BusLogic_ProbeInfoList[0],
			 &SavedProbeInfo[FlashPointCount],
			 MultiMasterCount * sizeof(BusLogic_ProbeInfo_T));
		  memcpy(&BusLogic_ProbeInfoList[MultiMasterCount],
			 &SavedProbeInfo[0],
			 FlashPointCount * sizeof(BusLogic_ProbeInfo_T));
		}
	    }
	}
    }
  else BusLogic_InitializeProbeInfoListISA(PrototypeHostAdapter);
}


#endif  /* CONFIG_PCI */


/*
  BusLogic_Failure prints a standardized error message, and then returns false.
*/

static boolean BusLogic_Failure(BusLogic_HostAdapter_T *HostAdapter,
				char *ErrorMessage)
{
  BusLogic_AnnounceDriver(HostAdapter);
  if (HostAdapter->HostAdapterBusType == BusLogic_PCI_Bus)
    {
      BusLogic_Error("While configuring BusLogic PCI Host Adapter at\n",
		     HostAdapter);
      BusLogic_Error("Bus %d Device %d I/O Address 0x%X PCI Address 0x%X:\n",
		     HostAdapter, HostAdapter->Bus, HostAdapter->Device,
		     HostAdapter->IO_Address, HostAdapter->PCI_Address);
    }
  else BusLogic_Error("While configuring BusLogic Host Adapter at "
		      "I/O Address 0x%X:\n", HostAdapter,
		      HostAdapter->IO_Address);
  BusLogic_Error("%s FAILED - DETACHING\n", HostAdapter, ErrorMessage);
  if (BusLogic_CommandFailureReason != NULL)
    BusLogic_Error("ADDITIONAL FAILURE INFO - %s\n", HostAdapter,
		   BusLogic_CommandFailureReason);
  return false;
}


/*
  BusLogic_ProbeHostAdapter probes for a BusLogic Host Adapter.
*/

static boolean BusLogic_ProbeHostAdapter(BusLogic_HostAdapter_T *HostAdapter)
{
  BusLogic_StatusRegister_T StatusRegister;
  BusLogic_InterruptRegister_T InterruptRegister;
  BusLogic_GeometryRegister_T GeometryRegister;
  /*
    FlashPoint Host Adapters are Probed by the FlashPoint SCCB Manager.
  */
  if (BusLogic_FlashPointHostAdapterP(HostAdapter))
    {
      FlashPoint_Info_T *FlashPointInfo = &HostAdapter->FlashPointInfo;
      FlashPointInfo->BaseAddress =
	(BusLogic_Base_Address_T) HostAdapter->IO_Address;
      FlashPointInfo->IRQ_Channel = HostAdapter->IRQ_Channel;
      FlashPointInfo->Present = false;
      if (!(FlashPoint_ProbeHostAdapter(FlashPointInfo) == 0 &&
	    FlashPointInfo->Present))
	{
	  BusLogic_Error("BusLogic: FlashPoint Host Adapter detected at "
			 "PCI Bus %d Device %d\n", HostAdapter,
			 HostAdapter->Bus, HostAdapter->Device);
	  BusLogic_Error("BusLogic: I/O Address 0x%X PCI Address 0x%X, "
			 "but FlashPoint\n", HostAdapter,
			 HostAdapter->IO_Address, HostAdapter->PCI_Address);
	  BusLogic_Error("BusLogic: Probe Function failed to validate it.\n",
			 HostAdapter);
	  return false;
	}
      if (BusLogic_GlobalOptions.TraceProbe)
	BusLogic_Notice("BusLogic_Probe(0x%X): FlashPoint Found\n",
			HostAdapter, HostAdapter->IO_Address);
      /*
	Indicate the Host Adapter Probe completed successfully.
      */
      return true;
    }
  /*
    Read the Status, Interrupt, and Geometry Registers to test if there are I/O
    ports that respond, and to check the values to determine if they are from a
    BusLogic Host Adapter.  A nonexistent I/O port will return 0xFF, in which
    case there is definitely no BusLogic Host Adapter at this base I/O Address.
    The test here is a subset of that used by the BusLogic Host Adapter BIOS.
  */
  StatusRegister.All = BusLogic_ReadStatusRegister(HostAdapter);
  InterruptRegister.All = BusLogic_ReadInterruptRegister(HostAdapter);
  GeometryRegister.All = BusLogic_ReadGeometryRegister(HostAdapter);
  if (BusLogic_GlobalOptions.TraceProbe)
    BusLogic_Notice("BusLogic_Probe(0x%X): Status 0x%02X, Interrupt 0x%02X, "
		    "Geometry 0x%02X\n", HostAdapter,
		    HostAdapter->IO_Address, StatusRegister.All,
		    InterruptRegister.All, GeometryRegister.All);
  if (StatusRegister.All == 0 ||
      StatusRegister.Bits.DiagnosticActive ||
      StatusRegister.Bits.CommandParameterRegisterBusy ||
      StatusRegister.Bits.Reserved ||
      StatusRegister.Bits.CommandInvalid ||
      InterruptRegister.Bits.Reserved != 0)
    return false;
  /*
    Check the undocumented Geometry Register to test if there is an I/O port
    that responded.  Adaptec Host Adapters do not implement the Geometry
    Register, so this test helps serve to avoid incorrectly recognizing an
    Adaptec 1542A or 1542B as a BusLogic.  Unfortunately, the Adaptec 1542C
    series does respond to the Geometry Register I/O port, but it will be
    rejected later when the Inquire Extended Setup Information command is
    issued in BusLogic_CheckHostAdapter.  The AMI FastDisk Host Adapter is a
    BusLogic clone that implements the same interface as earlier BusLogic
    Host Adapters, including the undocumented commands, and is therefore
    supported by this driver.  However, the AMI FastDisk always returns 0x00
    upon reading the Geometry Register, so the extended translation option
    should always be left disabled on the AMI FastDisk.
  */
  if (GeometryRegister.All == 0xFF) return false;
  /*
    Indicate the Host Adapter Probe completed successfully.
  */
  return true;
}


/*
  BusLogic_HardwareResetHostAdapter issues a Hardware Reset to the Host Adapter
  and waits for Host Adapter Diagnostics to complete.  If HardReset is true, a
  Hard Reset is performed which also initiates a SCSI Bus Reset.  Otherwise, a
  Soft Reset is performed which only resets the Host Adapter without forcing a
  SCSI Bus Reset.
*/

static boolean BusLogic_HardwareResetHostAdapter(BusLogic_HostAdapter_T
						   *HostAdapter,
						 boolean HardReset)
{
  BusLogic_StatusRegister_T StatusRegister;
  int TimeoutCounter;
  /*
    FlashPoint Host Adapters are Hard Reset by the FlashPoint SCCB Manager.
  */
  if (BusLogic_FlashPointHostAdapterP(HostAdapter))
    {
      FlashPoint_Info_T *FlashPointInfo = &HostAdapter->FlashPointInfo;
      FlashPointInfo->HostSoftReset = !HardReset;
      FlashPointInfo->ReportDataUnderrun = true;
      HostAdapter->CardHandle =
	FlashPoint_HardwareResetHostAdapter(FlashPointInfo);
      if (HostAdapter->CardHandle == FlashPoint_BadCardHandle) return false;
      /*
	Indicate the Host Adapter Hard Reset completed successfully.
      */
      return true;
    }
  /*
    Issue a Hard Reset or Soft Reset Command to the Host Adapter.  The Host
    Adapter should respond by setting Diagnostic Active in the Status Register.
  */
  if (HardReset)
    BusLogic_HardReset(HostAdapter);
  else BusLogic_SoftReset(HostAdapter);
  /*
    Wait until Diagnostic Active is set in the Status Register.
  */
  TimeoutCounter = 5*10000;
  while (--TimeoutCounter >= 0)
    {
      StatusRegister.All = BusLogic_ReadStatusRegister(HostAdapter);
      if (StatusRegister.Bits.DiagnosticActive) break;
      udelay(100);
    }
  if (BusLogic_GlobalOptions.TraceHardwareReset)
    BusLogic_Notice("BusLogic_HardwareReset(0x%X): Diagnostic Active, "
		    "Status 0x%02X\n", HostAdapter,
		    HostAdapter->IO_Address, StatusRegister.All);
  if (TimeoutCounter < 0) return false;
  /*
    Wait 100 microseconds to allow completion of any initial diagnostic
    activity which might leave the contents of the Status Register
    unpredictable.
  */
  udelay(100);
  /*
    Wait until Diagnostic Active is reset in the Status Register.
  */
  TimeoutCounter = 10*10000;
  while (--TimeoutCounter >= 0)
    {
      StatusRegister.All = BusLogic_ReadStatusRegister(HostAdapter);
      if (!StatusRegister.Bits.DiagnosticActive) break;
      udelay(100);
    }
  if (BusLogic_GlobalOptions.TraceHardwareReset)
    BusLogic_Notice("BusLogic_HardwareReset(0x%X): Diagnostic Completed, "
		    "Status 0x%02X\n", HostAdapter,
		    HostAdapter->IO_Address, StatusRegister.All);
  if (TimeoutCounter < 0) return false;
  /*
    Wait until at least one of the Diagnostic Failure, Host Adapter Ready,
    or Data In Register Ready bits is set in the Status Register.
  */
  TimeoutCounter = 10000;
  while (--TimeoutCounter >= 0)
    {
      StatusRegister.All = BusLogic_ReadStatusRegister(HostAdapter);
      if (StatusRegister.Bits.DiagnosticFailure ||
	  StatusRegister.Bits.HostAdapterReady ||
	  StatusRegister.Bits.DataInRegisterReady)
	break;
      udelay(100);
    }
  if (BusLogic_GlobalOptions.TraceHardwareReset)
    BusLogic_Notice("BusLogic_HardwareReset(0x%X): Host Adapter Ready, "
		    "Status 0x%02X\n", HostAdapter,
		    HostAdapter->IO_Address, StatusRegister.All);
  if (TimeoutCounter < 0) return false;
  /*
    If Diagnostic Failure is set or Host Adapter Ready is reset, then an
    error occurred during the Host Adapter diagnostics.  If Data In Register
    Ready is set, then there is an Error Code available.
  */
  if (StatusRegister.Bits.DiagnosticFailure ||
      !StatusRegister.Bits.HostAdapterReady)
    {
      BusLogic_CommandFailureReason = NULL;
      BusLogic_Failure(HostAdapter, "HARD RESET DIAGNOSTICS");
      BusLogic_Error("HOST ADAPTER STATUS REGISTER = %02X\n",
		     HostAdapter, StatusRegister.All);
      if (StatusRegister.Bits.DataInRegisterReady)
	{
	  unsigned char ErrorCode = BusLogic_ReadDataInRegister(HostAdapter);
	  BusLogic_Error("HOST ADAPTER ERROR CODE = %d\n",
			 HostAdapter, ErrorCode);
	}
      return false;
    }
  /*
    Indicate the Host Adapter Hard Reset completed successfully.
  */
  return true;
}


/*
  BusLogic_CheckHostAdapter checks to be sure this really is a BusLogic
  Host Adapter.
*/

static boolean BusLogic_CheckHostAdapter(BusLogic_HostAdapter_T *HostAdapter)
{
  BusLogic_ExtendedSetupInformation_T ExtendedSetupInformation;
  BusLogic_RequestedReplyLength_T RequestedReplyLength;
  boolean Result = true;
  /*
    FlashPoint Host Adapters do not require this protection.
  */
  if (BusLogic_FlashPointHostAdapterP(HostAdapter)) return true;
  /*
    Issue the Inquire Extended Setup Information command.  Only genuine
    BusLogic Host Adapters and true clones support this command.  Adaptec 1542C
    series Host Adapters that respond to the Geometry Register I/O port will
    fail this command.
  */
  RequestedReplyLength = sizeof(ExtendedSetupInformation);
  if (BusLogic_Command(HostAdapter,
		       BusLogic_InquireExtendedSetupInformation,
		       &RequestedReplyLength,
		       sizeof(RequestedReplyLength),
		       &ExtendedSetupInformation,
		       sizeof(ExtendedSetupInformation))
      != sizeof(ExtendedSetupInformation))
    Result = false;
  /*
    Provide tracing information if requested and return.
  */
  if (BusLogic_GlobalOptions.TraceProbe)
    BusLogic_Notice("BusLogic_Check(0x%X): MultiMaster %s\n", HostAdapter,
		    HostAdapter->IO_Address, (Result ? "Found" : "Not Found"));
  return Result;
}


/*
  BusLogic_ReadHostAdapterConfiguration reads the Configuration Information
  from Host Adapter and initializes the Host Adapter structure.
*/

static boolean BusLogic_ReadHostAdapterConfiguration(BusLogic_HostAdapter_T
						     *HostAdapter)
{
  BusLogic_BoardID_T BoardID;
  BusLogic_Configuration_T Configuration;
  BusLogic_SetupInformation_T SetupInformation;
  BusLogic_ExtendedSetupInformation_T ExtendedSetupInformation;
  BusLogic_HostAdapterModelNumber_T HostAdapterModelNumber;
  BusLogic_FirmwareVersion3rdDigit_T FirmwareVersion3rdDigit;
  BusLogic_FirmwareVersionLetter_T FirmwareVersionLetter;
  BusLogic_PCIHostAdapterInformation_T PCIHostAdapterInformation;
  BusLogic_FetchHostAdapterLocalRAMRequest_T FetchHostAdapterLocalRAMRequest;
  BusLogic_AutoSCSIData_T AutoSCSIData;
  BusLogic_GeometryRegister_T GeometryRegister;
  BusLogic_RequestedReplyLength_T RequestedReplyLength;
  unsigned char *TargetPointer, Character;
  int TargetID, i;
  /*
    Configuration Information for FlashPoint Host Adapters is provided in the
    FlashPoint_Info structure by the FlashPoint SCCB Manager's Probe Function.
    Initialize fields in the Host Adapter structure from the FlashPoint_Info
    structure.
  */
  if (BusLogic_FlashPointHostAdapterP(HostAdapter))
    {
      FlashPoint_Info_T *FlashPointInfo = &HostAdapter->FlashPointInfo;
      TargetPointer = HostAdapter->ModelName;
      *TargetPointer++ = 'B';
      *TargetPointer++ = 'T';
      *TargetPointer++ = '-';
      for (i = 0; i < sizeof(FlashPointInfo->ModelNumber); i++)
	*TargetPointer++ = FlashPointInfo->ModelNumber[i];
      *TargetPointer++ = '\0';
      strcpy(HostAdapter->FirmwareVersion, FlashPoint_FirmwareVersion);
      HostAdapter->SCSI_ID = FlashPointInfo->SCSI_ID;
      HostAdapter->ExtendedTranslationEnabled =
	FlashPointInfo->ExtendedTranslationEnabled;
      HostAdapter->ParityCheckingEnabled =
	FlashPointInfo->ParityCheckingEnabled;
      HostAdapter->BusResetEnabled = !FlashPointInfo->HostSoftReset;
      HostAdapter->LevelSensitiveInterrupt = true;
      HostAdapter->HostWideSCSI = FlashPointInfo->HostWideSCSI;
      HostAdapter->HostDifferentialSCSI = false;
      HostAdapter->HostSupportsSCAM = true;
      HostAdapter->HostUltraSCSI = true;
      HostAdapter->ExtendedLUNSupport = true;
      HostAdapter->TerminationInfoValid = true;
      HostAdapter->LowByteTerminated = FlashPointInfo->LowByteTerminated;
      HostAdapter->HighByteTerminated = FlashPointInfo->HighByteTerminated;
      HostAdapter->SCAM_Enabled = FlashPointInfo->SCAM_Enabled;
      HostAdapter->SCAM_Level2 = FlashPointInfo->SCAM_Level2;
      HostAdapter->DriverScatterGatherLimit = BusLogic_ScatterGatherLimit;
      HostAdapter->MaxTargetDevices = (HostAdapter->HostWideSCSI ? 16 : 8);
      HostAdapter->MaxLogicalUnits = 32;
      HostAdapter->InitialCCBs = 4 * BusLogic_CCB_AllocationGroupSize;
      HostAdapter->IncrementalCCBs = BusLogic_CCB_AllocationGroupSize;
      HostAdapter->DriverQueueDepth = 255;
      HostAdapter->HostAdapterQueueDepth = HostAdapter->DriverQueueDepth;
      HostAdapter->SynchronousPermitted = FlashPointInfo->SynchronousPermitted;
      HostAdapter->FastPermitted = FlashPointInfo->FastPermitted;
      HostAdapter->UltraPermitted = FlashPointInfo->UltraPermitted;
      HostAdapter->WidePermitted = FlashPointInfo->WidePermitted;
      HostAdapter->DisconnectPermitted = FlashPointInfo->DisconnectPermitted;
      HostAdapter->TaggedQueuingPermitted = 0xFFFF;
      goto Common;
    }
  /*
    Issue the Inquire Board ID command.
  */
  if (BusLogic_Command(HostAdapter, BusLogic_InquireBoardID, NULL, 0,
		       &BoardID, sizeof(BoardID)) != sizeof(BoardID))
    return BusLogic_Failure(HostAdapter, "INQUIRE BOARD ID");
  /*
    Issue the Inquire Configuration command.
  */
  if (BusLogic_Command(HostAdapter, BusLogic_InquireConfiguration, NULL, 0,
		       &Configuration, sizeof(Configuration))
      != sizeof(Configuration))
    return BusLogic_Failure(HostAdapter, "INQUIRE CONFIGURATION");
  /*
    Issue the Inquire Setup Information command.
  */
  RequestedReplyLength = sizeof(SetupInformation);
  if (BusLogic_Command(HostAdapter, BusLogic_InquireSetupInformation,
		       &RequestedReplyLength, sizeof(RequestedReplyLength),
		       &SetupInformation, sizeof(SetupInformation))
      != sizeof(SetupInformation))
    return BusLogic_Failure(HostAdapter, "INQUIRE SETUP INFORMATION");
  /*
    Issue the Inquire Extended Setup Information command.
  */
  RequestedReplyLength = sizeof(ExtendedSetupInformation);
  if (BusLogic_Command(HostAdapter, BusLogic_InquireExtendedSetupInformation,
		       &RequestedReplyLength, sizeof(RequestedReplyLength),
		       &ExtendedSetupInformation,
		       sizeof(ExtendedSetupInformation))
      != sizeof(ExtendedSetupInformation))
    return BusLogic_Failure(HostAdapter, "INQUIRE EXTENDED SETUP INFORMATION");
  /*
    Issue the Inquire Firmware Version 3rd Digit command.
  */
  FirmwareVersion3rdDigit = '\0';
  if (BoardID.FirmwareVersion1stDigit > '0')
    if (BusLogic_Command(HostAdapter, BusLogic_InquireFirmwareVersion3rdDigit,
			 NULL, 0, &FirmwareVersion3rdDigit,
			 sizeof(FirmwareVersion3rdDigit))
	!= sizeof(FirmwareVersion3rdDigit))
      return BusLogic_Failure(HostAdapter, "INQUIRE FIRMWARE 3RD DIGIT");
  /*
    Issue the Inquire Host Adapter Model Number command.
  */
  if (ExtendedSetupInformation.BusType == 'A' &&
      BoardID.FirmwareVersion1stDigit == '2')
    /* BusLogic BT-542B ISA 2.xx */
    strcpy(HostAdapterModelNumber, "542B");
  else if (ExtendedSetupInformation.BusType == 'E' &&
	   BoardID.FirmwareVersion1stDigit == '2' &&
	   (BoardID.FirmwareVersion2ndDigit <= '1' ||
	    (BoardID.FirmwareVersion2ndDigit == '2' &&
	     FirmwareVersion3rdDigit == '0')))
    /* BusLogic BT-742A EISA 2.1x or 2.20 */
    strcpy(HostAdapterModelNumber, "742A");
  else if (ExtendedSetupInformation.BusType == 'E' &&
	   BoardID.FirmwareVersion1stDigit == '0')
    /* AMI FastDisk EISA Series 441 0.x */
    strcpy(HostAdapterModelNumber, "747A");
  else
    {
      RequestedReplyLength = sizeof(HostAdapterModelNumber);
      if (BusLogic_Command(HostAdapter, BusLogic_InquireHostAdapterModelNumber,
			   &RequestedReplyLength, sizeof(RequestedReplyLength),
			   &HostAdapterModelNumber,
			   sizeof(HostAdapterModelNumber))
	  != sizeof(HostAdapterModelNumber))
	return BusLogic_Failure(HostAdapter,
				"INQUIRE HOST ADAPTER MODEL NUMBER");
    }
  /*
    BusLogic MultiMaster Host Adapters can be identified by their model number
    and the major version number of their firmware as follows:

    5.xx	BusLogic "W" Series Host Adapters:
		  BT-948/958/958D
    4.xx	BusLogic "C" Series Host Adapters:
		  BT-946C/956C/956CD/747C/757C/757CD/445C/545C/540CF
    3.xx	BusLogic "S" Series Host Adapters:
		  BT-747S/747D/757S/757D/445S/545S/542D
		  BT-542B/742A (revision H)
    2.xx	BusLogic "A" Series Host Adapters:
		  BT-542B/742A (revision G and below)
    0.xx	AMI FastDisk VLB/EISA BusLogic Clone Host Adapter
  */
  /*
    Save the Model Name and Host Adapter Name in the Host Adapter structure.
  */
  TargetPointer = HostAdapter->ModelName;
  *TargetPointer++ = 'B';
  *TargetPointer++ = 'T';
  *TargetPointer++ = '-';
  for (i = 0; i < sizeof(HostAdapterModelNumber); i++)
    {
      Character = HostAdapterModelNumber[i];
      if (Character == ' ' || Character == '\0') break;
      *TargetPointer++ = Character;
    }
  *TargetPointer++ = '\0';
  /*
    Save the Firmware Version in the Host Adapter structure.
  */
  TargetPointer = HostAdapter->FirmwareVersion;
  *TargetPointer++ = BoardID.FirmwareVersion1stDigit;
  *TargetPointer++ = '.';
  *TargetPointer++ = BoardID.FirmwareVersion2ndDigit;
  if (FirmwareVersion3rdDigit != ' ' && FirmwareVersion3rdDigit != '\0')
    *TargetPointer++ = FirmwareVersion3rdDigit;
  *TargetPointer = '\0';
  /*
    Issue the Inquire Firmware Version Letter command.
  */
  if (strcmp(HostAdapter->FirmwareVersion, "3.3") >= 0)
    {
      if (BusLogic_Command(HostAdapter, BusLogic_InquireFirmwareVersionLetter,
			   NULL, 0, &FirmwareVersionLetter,
			   sizeof(FirmwareVersionLetter))
	  != sizeof(FirmwareVersionLetter))
	return BusLogic_Failure(HostAdapter,
				"INQUIRE FIRMWARE VERSION LETTER");
      if (FirmwareVersionLetter != ' ' && FirmwareVersionLetter != '\0')
	*TargetPointer++ = FirmwareVersionLetter;
      *TargetPointer = '\0';
    }
  /*
    Save the Host Adapter SCSI ID in the Host Adapter structure.
  */
  HostAdapter->SCSI_ID = Configuration.HostAdapterID;
  /*
    Determine the Bus Type and save it in the Host Adapter structure, determine
    and save the IRQ Channel if necessary, and determine and save the DMA
    Channel for ISA Host Adapters.
  */
  HostAdapter->HostAdapterBusType =
    BusLogic_HostAdapterBusTypes[HostAdapter->ModelName[3] - '4'];
  if (HostAdapter->IRQ_Channel == 0)
    {
      if (Configuration.IRQ_Channel9)
	HostAdapter->IRQ_Channel = 9;
      else if (Configuration.IRQ_Channel10)
	HostAdapter->IRQ_Channel = 10;
      else if (Configuration.IRQ_Channel11)
	HostAdapter->IRQ_Channel = 11;
      else if (Configuration.IRQ_Channel12)
	HostAdapter->IRQ_Channel = 12;
      else if (Configuration.IRQ_Channel14)
	HostAdapter->IRQ_Channel = 14;
      else if (Configuration.IRQ_Channel15)
	HostAdapter->IRQ_Channel = 15;
    }
  if (HostAdapter->HostAdapterBusType == BusLogic_ISA_Bus)
    {
      if (Configuration.DMA_Channel5)
	HostAdapter->DMA_Channel = 5;
      else if (Configuration.DMA_Channel6)
	HostAdapter->DMA_Channel = 6;
      else if (Configuration.DMA_Channel7)
	HostAdapter->DMA_Channel = 7;
    }
  /*
    Determine whether Extended Translation is enabled and save it in
    the Host Adapter structure.
  */
  GeometryRegister.All = BusLogic_ReadGeometryRegister(HostAdapter);
  HostAdapter->ExtendedTranslationEnabled =
    GeometryRegister.Bits.ExtendedTranslationEnabled;
  /*
    Save the Scatter Gather Limits, Level Sensitive Interrupt flag, Wide
    SCSI flag, Differential SCSI flag, SCAM Supported flag, and
    Ultra SCSI flag in the Host Adapter structure.
  */
  HostAdapter->HostAdapterScatterGatherLimit =
    ExtendedSetupInformation.ScatterGatherLimit;
  HostAdapter->DriverScatterGatherLimit =
    HostAdapter->HostAdapterScatterGatherLimit;
  if (HostAdapter->HostAdapterScatterGatherLimit > BusLogic_ScatterGatherLimit)
    HostAdapter->DriverScatterGatherLimit = BusLogic_ScatterGatherLimit;
  if (ExtendedSetupInformation.Misc.LevelSensitiveInterrupt)
    HostAdapter->LevelSensitiveInterrupt = true;
  HostAdapter->HostWideSCSI = ExtendedSetupInformation.HostWideSCSI;
  HostAdapter->HostDifferentialSCSI =
    ExtendedSetupInformation.HostDifferentialSCSI;
  HostAdapter->HostSupportsSCAM = ExtendedSetupInformation.HostSupportsSCAM;
  HostAdapter->HostUltraSCSI = ExtendedSetupInformation.HostUltraSCSI;
  /*
    Determine whether Extended LUN Format CCBs are supported and save the
    information in the Host Adapter structure.
  */
  if (HostAdapter->FirmwareVersion[0] == '5' ||
      (HostAdapter->FirmwareVersion[0] == '4' && HostAdapter->HostWideSCSI))
    HostAdapter->ExtendedLUNSupport = true;
  /*
    Issue the Inquire PCI Host Adapter Information command to read the
    Termination Information from "W" series MultiMaster Host Adapters.
  */
  if (HostAdapter->FirmwareVersion[0] == '5')
    {
      if (BusLogic_Command(HostAdapter,
			   BusLogic_InquirePCIHostAdapterInformation,
			   NULL, 0, &PCIHostAdapterInformation,
			   sizeof(PCIHostAdapterInformation))
	  != sizeof(PCIHostAdapterInformation))
	return BusLogic_Failure(HostAdapter,
				"INQUIRE PCI HOST ADAPTER INFORMATION");
      /*
	Save the Termination Information in the Host Adapter structure.
      */
      if (PCIHostAdapterInformation.GenericInfoValid)
	{
	  HostAdapter->TerminationInfoValid = true;
	  HostAdapter->LowByteTerminated =
	    PCIHostAdapterInformation.LowByteTerminated;
	  HostAdapter->HighByteTerminated =
	    PCIHostAdapterInformation.HighByteTerminated;
	}
    }
  /*
    Issue the Fetch Host Adapter Local RAM command to read the AutoSCSI data
    from "W" and "C" series MultiMaster Host Adapters.
  */
  if (HostAdapter->FirmwareVersion[0] >= '4')
    {
      FetchHostAdapterLocalRAMRequest.ByteOffset =
	BusLogic_AutoSCSI_BaseOffset;
      FetchHostAdapterLocalRAMRequest.ByteCount = sizeof(AutoSCSIData);
      if (BusLogic_Command(HostAdapter,
			   BusLogic_FetchHostAdapterLocalRAM,
			   &FetchHostAdapterLocalRAMRequest,
			   sizeof(FetchHostAdapterLocalRAMRequest),
			   &AutoSCSIData, sizeof(AutoSCSIData))
	  != sizeof(AutoSCSIData))
	return BusLogic_Failure(HostAdapter, "FETCH HOST ADAPTER LOCAL RAM");
      /*
	Save the Parity Checking Enabled, Bus Reset Enabled, and Termination
	Information in the Host Adapter structure.
      */
      HostAdapter->ParityCheckingEnabled = AutoSCSIData.ParityCheckingEnabled;
      HostAdapter->BusResetEnabled = AutoSCSIData.BusResetEnabled;
      if (HostAdapter->FirmwareVersion[0] == '4')
	{
	  HostAdapter->TerminationInfoValid = true;
	  HostAdapter->LowByteTerminated = AutoSCSIData.LowByteTerminated;
	  HostAdapter->HighByteTerminated = AutoSCSIData.HighByteTerminated;
	}
      /*
	Save the Wide Permitted, Fast Permitted, Synchronous Permitted,
	Disconnect Permitted, Ultra Permitted, and SCAM Information in the
	Host Adapter structure.
      */
      HostAdapter->WidePermitted = AutoSCSIData.WidePermitted;
      HostAdapter->FastPermitted = AutoSCSIData.FastPermitted;
      HostAdapter->SynchronousPermitted =
	AutoSCSIData.SynchronousPermitted;
      HostAdapter->DisconnectPermitted =
	AutoSCSIData.DisconnectPermitted;
      if (HostAdapter->HostUltraSCSI)
	HostAdapter->UltraPermitted = AutoSCSIData.UltraPermitted;
      if (HostAdapter->HostSupportsSCAM)
	{
	  HostAdapter->SCAM_Enabled = AutoSCSIData.SCAM_Enabled;
	  HostAdapter->SCAM_Level2 = AutoSCSIData.SCAM_Level2;
	}
    }
  /*
    Initialize fields in the Host Adapter structure for "S" and "A" series
    MultiMaster Host Adapters.
  */
  if (HostAdapter->FirmwareVersion[0] < '4')
    {
      if (SetupInformation.SynchronousInitiationEnabled)
	{
	  HostAdapter->SynchronousPermitted = 0xFF;
	  if (HostAdapter->HostAdapterBusType == BusLogic_EISA_Bus)
	    {
	      if (ExtendedSetupInformation.Misc.FastOnEISA)
		HostAdapter->FastPermitted = 0xFF;
	      if (strcmp(HostAdapter->ModelName, "BT-757") == 0)
		HostAdapter->WidePermitted = 0xFF;
	    }
	}
      HostAdapter->DisconnectPermitted = 0xFF;
      HostAdapter->ParityCheckingEnabled =
	SetupInformation.ParityCheckingEnabled;
      HostAdapter->BusResetEnabled = true;
    }
  /*
    Determine the maximum number of Target IDs and Logical Units supported by
    this driver for Wide and Narrow Host Adapters.
  */
  HostAdapter->MaxTargetDevices = (HostAdapter->HostWideSCSI ? 16 : 8);
  HostAdapter->MaxLogicalUnits = (HostAdapter->ExtendedLUNSupport ? 32 : 8);
  /*
    Select appropriate values for the Mailbox Count, Driver Queue Depth,
    Initial CCBs, and Incremental CCBs variables based on whether or not Strict
    Round Robin Mode is supported.  If Strict Round Robin Mode is supported,
    then there is no performance degradation in using the maximum possible
    number of Outgoing and Incoming Mailboxes and allowing the Tagged and
    Untagged Queue Depths to determine the actual utilization.  If Strict Round
    Robin Mode is not supported, then the Host Adapter must scan all the
    Outgoing Mailboxes whenever an Outgoing Mailbox entry is made, which can
    cause a substantial performance penalty.  The host adapters actually have
    room to store the following number of CCBs internally; that is, they can
    internally queue and manage this many active commands on the SCSI bus
    simultaneously.  Performance measurements demonstrate that the Driver Queue
    Depth should be set to the Mailbox Count, rather than the Host Adapter
    Queue Depth (internal CCB capacity), as it is more efficient to have the
    queued commands waiting in Outgoing Mailboxes if necessary than to block
    the process in the higher levels of the SCSI Subsystem.

	192	  BT-948/958/958D
	100	  BT-946C/956C/956CD/747C/757C/757CD/445C
	 50	  BT-545C/540CF
	 30	  BT-747S/747D/757S/757D/445S/545S/542D/542B/742A
  */
  if (HostAdapter->FirmwareVersion[0] == '5')
    HostAdapter->HostAdapterQueueDepth = 192;
  else if (HostAdapter->FirmwareVersion[0] == '4')
    HostAdapter->HostAdapterQueueDepth =
      (HostAdapter->HostAdapterBusType != BusLogic_ISA_Bus ? 100 : 50);
  else HostAdapter->HostAdapterQueueDepth = 30;
  if (strcmp(HostAdapter->FirmwareVersion, "3.31") >= 0)
    {
      HostAdapter->StrictRoundRobinModeSupport = true;
      HostAdapter->MailboxCount = BusLogic_MaxMailboxes;
    }
  else
    {
      HostAdapter->StrictRoundRobinModeSupport = false;
      HostAdapter->MailboxCount = 32;
    }
  HostAdapter->DriverQueueDepth = HostAdapter->MailboxCount;
  HostAdapter->InitialCCBs = 4 * BusLogic_CCB_AllocationGroupSize;
  HostAdapter->IncrementalCCBs = BusLogic_CCB_AllocationGroupSize;
  /*
    Tagged Queuing support is available and operates properly on all "W" series
    MultiMaster Host Adapters, on "C" series MultiMaster Host Adapters with
    firmware version 4.22 and above, and on "S" series MultiMaster Host
    Adapters with firmware version 3.35 and above.
  */
  HostAdapter->TaggedQueuingPermitted = 0;
  switch (HostAdapter->FirmwareVersion[0])
    {
    case '5':
      HostAdapter->TaggedQueuingPermitted = 0xFFFF;
      break;
    case '4':
      if (strcmp(HostAdapter->FirmwareVersion, "4.22") >= 0)
	HostAdapter->TaggedQueuingPermitted = 0xFFFF;
      break;
    case '3':
      if (strcmp(HostAdapter->FirmwareVersion, "3.35") >= 0)
	HostAdapter->TaggedQueuingPermitted = 0xFFFF;
      break;
    }
  /*
    Determine the Host Adapter BIOS Address if the BIOS is enabled and
    save it in the Host Adapter structure.  The BIOS is disabled if the
    BIOS_Address is 0.
  */
  HostAdapter->BIOS_Address = ExtendedSetupInformation.BIOS_Address << 12;
  /*
    ISA Host Adapters require Bounce Buffers if there is more than 16MB memory.
  */
  if (HostAdapter->HostAdapterBusType == BusLogic_ISA_Bus &&
      (void *) high_memory > (void *) MAX_DMA_ADDRESS)
    HostAdapter->BounceBuffersRequired = true;
  /*
    BusLogic BT-445S Host Adapters prior to board revision E have a hardware
    bug whereby when the BIOS is enabled, transfers to/from the same address
    range the BIOS occupies modulo 16MB are handled incorrectly.  Only properly
    functioning BT-445S Host Adapters have firmware version 3.37, so require
    that ISA Bounce Buffers be used for the buggy BT-445S models if there is
    more than 16MB memory.
  */
  if (HostAdapter->BIOS_Address > 0 &&
      strcmp(HostAdapter->ModelName, "BT-445S") == 0 &&
      strcmp(HostAdapter->FirmwareVersion, "3.37") < 0 &&
      (void *) high_memory > (void *) MAX_DMA_ADDRESS)
    HostAdapter->BounceBuffersRequired = true;
  /*
    Initialize parameters common to MultiMaster and FlashPoint Host Adapters.
  */
Common:
  /*
    Initialize the Host Adapter Full Model Name from the Model Name.
  */
  strcpy(HostAdapter->FullModelName, "BusLogic ");
  strcat(HostAdapter->FullModelName, HostAdapter->ModelName);
  /*
    Select an appropriate value for the Tagged Queue Depth either from a
    BusLogic Driver Options specification, or based on whether this Host
    Adapter requires that ISA Bounce Buffers be used.  The Tagged Queue Depth
    is left at 0 for automatic determination in BusLogic_SelectQueueDepths.
    Initialize the Untagged Queue Depth.
  */
  for (TargetID = 0; TargetID < BusLogic_MaxTargetDevices; TargetID++)
    {
      unsigned char QueueDepth = 0;
      if (HostAdapter->DriverOptions != NULL &&
	  HostAdapter->DriverOptions->QueueDepth[TargetID] > 0)
	QueueDepth = HostAdapter->DriverOptions->QueueDepth[TargetID];
      else if (HostAdapter->BounceBuffersRequired)
	QueueDepth = BusLogic_TaggedQueueDepthBB;
      HostAdapter->QueueDepth[TargetID] = QueueDepth;
    }
  if (HostAdapter->BounceBuffersRequired)
    HostAdapter->UntaggedQueueDepth = BusLogic_UntaggedQueueDepthBB;
  else HostAdapter->UntaggedQueueDepth = BusLogic_UntaggedQueueDepth;
  if (HostAdapter->DriverOptions != NULL)
    HostAdapter->CommonQueueDepth =
      HostAdapter->DriverOptions->CommonQueueDepth;
  if (HostAdapter->CommonQueueDepth > 0 &&
      HostAdapter->CommonQueueDepth < HostAdapter->UntaggedQueueDepth)
    HostAdapter->UntaggedQueueDepth = HostAdapter->CommonQueueDepth;
  /*
    Tagged Queuing is only allowed if Disconnect/Reconnect is permitted.
    Therefore, mask the Tagged Queuing Permitted Default bits with the
    Disconnect/Reconnect Permitted bits.
  */
  HostAdapter->TaggedQueuingPermitted &= HostAdapter->DisconnectPermitted;
  /*
    Combine the default Tagged Queuing Permitted bits with any BusLogic Driver
    Options Tagged Queuing specification.
  */
  if (HostAdapter->DriverOptions != NULL)
    HostAdapter->TaggedQueuingPermitted =
      (HostAdapter->DriverOptions->TaggedQueuingPermitted &
       HostAdapter->DriverOptions->TaggedQueuingPermittedMask) |
      (HostAdapter->TaggedQueuingPermitted &
       ~HostAdapter->DriverOptions->TaggedQueuingPermittedMask);
  /*
    Select appropriate values for the Error Recovery Strategy array
    either from a BusLogic Driver Options specification, or using
    BusLogic_ErrorRecovery_Default.
  */
  for (TargetID = 0; TargetID < BusLogic_MaxTargetDevices; TargetID++)
    if (HostAdapter->DriverOptions != NULL)
      HostAdapter->ErrorRecoveryStrategy[TargetID] =
	HostAdapter->DriverOptions->ErrorRecoveryStrategy[TargetID];
    else HostAdapter->ErrorRecoveryStrategy[TargetID] =
	   BusLogic_ErrorRecovery_Default;
  /*
    Select an appropriate value for Bus Settle Time either from a BusLogic
    Driver Options specification, or from BusLogic_DefaultBusSettleTime.
  */
  if (HostAdapter->DriverOptions != NULL &&
      HostAdapter->DriverOptions->BusSettleTime > 0)
    HostAdapter->BusSettleTime = HostAdapter->DriverOptions->BusSettleTime;
  else HostAdapter->BusSettleTime = BusLogic_DefaultBusSettleTime;
  /*
    Indicate reading the Host Adapter Configuration completed successfully.
  */
  return true;
}


/*
  BusLogic_ReportHostAdapterConfiguration reports the configuration of
  Host Adapter.
*/

static boolean BusLogic_ReportHostAdapterConfiguration(BusLogic_HostAdapter_T
						       *HostAdapter)
{
  unsigned short AllTargetsMask = (1 << HostAdapter->MaxTargetDevices) - 1;
  unsigned short SynchronousPermitted, FastPermitted;
  unsigned short UltraPermitted, WidePermitted;
  unsigned short DisconnectPermitted, TaggedQueuingPermitted;
  boolean CommonSynchronousNegotiation, CommonTaggedQueueDepth;
  boolean CommonErrorRecovery;
  char SynchronousString[BusLogic_MaxTargetDevices+1];
  char WideString[BusLogic_MaxTargetDevices+1];
  char DisconnectString[BusLogic_MaxTargetDevices+1];
  char TaggedQueuingString[BusLogic_MaxTargetDevices+1];
  char ErrorRecoveryString[BusLogic_MaxTargetDevices+1];
  char *SynchronousMessage = SynchronousString;
  char *WideMessage = WideString;
  char *DisconnectMessage = DisconnectString;
  char *TaggedQueuingMessage = TaggedQueuingString;
  char *ErrorRecoveryMessage = ErrorRecoveryString;
  int TargetID;
  BusLogic_Info("Configuring BusLogic Model %s %s%s%s%s SCSI Host Adapter\n",
		HostAdapter, HostAdapter->ModelName,
		BusLogic_HostAdapterBusNames[HostAdapter->HostAdapterBusType],
		(HostAdapter->HostWideSCSI ? " Wide" : ""),
		(HostAdapter->HostDifferentialSCSI ? " Differential" : ""),
		(HostAdapter->HostUltraSCSI ? " Ultra" : ""));
  BusLogic_Info("  Firmware Version: %s, I/O Address: 0x%X, "
		"IRQ Channel: %d/%s\n", HostAdapter,
		HostAdapter->FirmwareVersion,
		HostAdapter->IO_Address, HostAdapter->IRQ_Channel,
		(HostAdapter->LevelSensitiveInterrupt ? "Level" : "Edge"));
  if (HostAdapter->HostAdapterBusType != BusLogic_PCI_Bus)
    {
      BusLogic_Info("  DMA Channel: ", HostAdapter);
      if (HostAdapter->DMA_Channel > 0)
	BusLogic_Info("%d, ", HostAdapter, HostAdapter->DMA_Channel);
      else BusLogic_Info("None, ", HostAdapter);
      if (HostAdapter->BIOS_Address > 0)
	BusLogic_Info("BIOS Address: 0x%X, ", HostAdapter,
		      HostAdapter->BIOS_Address);
      else BusLogic_Info("BIOS Address: None, ", HostAdapter);
    }
  else
    {
      BusLogic_Info("  PCI Bus: %d, Device: %d, Address: ",
		    HostAdapter, HostAdapter->Bus, HostAdapter->Device);
      if (HostAdapter->PCI_Address > 0)
	BusLogic_Info("0x%X, ", HostAdapter, HostAdapter->PCI_Address);
      else BusLogic_Info("Unassigned, ", HostAdapter);
    }
  BusLogic_Info("Host Adapter SCSI ID: %d\n", HostAdapter,
		HostAdapter->SCSI_ID);
  BusLogic_Info("  Parity Checking: %s, Extended Translation: %s\n",
		HostAdapter,
		(HostAdapter->ParityCheckingEnabled
		 ? "Enabled" : "Disabled"),
		(HostAdapter->ExtendedTranslationEnabled
		 ? "Enabled" : "Disabled"));
  AllTargetsMask &= ~(1 << HostAdapter->SCSI_ID);
  SynchronousPermitted = HostAdapter->SynchronousPermitted & AllTargetsMask;
  FastPermitted = HostAdapter->FastPermitted & AllTargetsMask;
  UltraPermitted = HostAdapter->UltraPermitted & AllTargetsMask;
  if ((BusLogic_MultiMasterHostAdapterP(HostAdapter) &&
       (HostAdapter->FirmwareVersion[0] >= '4' ||
	HostAdapter->HostAdapterBusType == BusLogic_EISA_Bus)) ||
      BusLogic_FlashPointHostAdapterP(HostAdapter))
    {
      CommonSynchronousNegotiation = false;
      if (SynchronousPermitted == 0)
	{
	  SynchronousMessage = "Disabled";
	  CommonSynchronousNegotiation = true;
	}
      else if (SynchronousPermitted == AllTargetsMask)
	{
	  if (FastPermitted == 0)
	    {
	      SynchronousMessage = "Slow";
	      CommonSynchronousNegotiation = true;
	    }
	  else if (FastPermitted == AllTargetsMask)
	    {
	      if (UltraPermitted == 0)
		{
		  SynchronousMessage = "Fast";
		  CommonSynchronousNegotiation = true;
		}
	      else if (UltraPermitted == AllTargetsMask)
		{
		  SynchronousMessage = "Ultra";
		  CommonSynchronousNegotiation = true;
		}
	    }
	}
      if (!CommonSynchronousNegotiation)
	{
	  for (TargetID = 0;
	       TargetID < HostAdapter->MaxTargetDevices;
	       TargetID++)
	    SynchronousString[TargetID] =
	      ((!(SynchronousPermitted & (1 << TargetID))) ? 'N' :
	       (!(FastPermitted & (1 << TargetID)) ? 'S' :
		(!(UltraPermitted & (1 << TargetID)) ? 'F' : 'U')));
	  SynchronousString[HostAdapter->SCSI_ID] = '#';
	  SynchronousString[HostAdapter->MaxTargetDevices] = '\0';
	}
    }
  else SynchronousMessage =
	 (SynchronousPermitted == 0 ? "Disabled" : "Enabled");
  WidePermitted = HostAdapter->WidePermitted & AllTargetsMask;
  if (WidePermitted == 0)
    WideMessage = "Disabled";
  else if (WidePermitted == AllTargetsMask)
    WideMessage = "Enabled";
  else
    {
      for (TargetID = 0; TargetID < HostAdapter->MaxTargetDevices; TargetID++)
	 WideString[TargetID] =
	   ((WidePermitted & (1 << TargetID)) ? 'Y' : 'N');
      WideString[HostAdapter->SCSI_ID] = '#';
      WideString[HostAdapter->MaxTargetDevices] = '\0';
    }
  DisconnectPermitted = HostAdapter->DisconnectPermitted & AllTargetsMask;
  if (DisconnectPermitted == 0)
    DisconnectMessage = "Disabled";
  else if (DisconnectPermitted == AllTargetsMask)
    DisconnectMessage = "Enabled";
  else
    {
      for (TargetID = 0; TargetID < HostAdapter->MaxTargetDevices; TargetID++)
	DisconnectString[TargetID] =
	  ((DisconnectPermitted & (1 << TargetID)) ? 'Y' : 'N');
      DisconnectString[HostAdapter->SCSI_ID] = '#';
      DisconnectString[HostAdapter->MaxTargetDevices] = '\0';
    }
  TaggedQueuingPermitted =
    HostAdapter->TaggedQueuingPermitted & AllTargetsMask;
  if (TaggedQueuingPermitted == 0)
    TaggedQueuingMessage = "Disabled";
  else if (TaggedQueuingPermitted == AllTargetsMask)
    TaggedQueuingMessage = "Enabled";
  else
    {
      for (TargetID = 0; TargetID < HostAdapter->MaxTargetDevices; TargetID++)
	TaggedQueuingString[TargetID] =
	  ((TaggedQueuingPermitted & (1 << TargetID)) ? 'Y' : 'N');
      TaggedQueuingString[HostAdapter->SCSI_ID] = '#';
      TaggedQueuingString[HostAdapter->MaxTargetDevices] = '\0';
    }
  BusLogic_Info("  Synchronous Negotiation: %s, Wide Negotiation: %s\n",
		HostAdapter, SynchronousMessage, WideMessage);
  BusLogic_Info("  Disconnect/Reconnect: %s, Tagged Queuing: %s\n",
		HostAdapter, DisconnectMessage, TaggedQueuingMessage);
  if (BusLogic_MultiMasterHostAdapterP(HostAdapter))
    {
      BusLogic_Info("  Scatter/Gather Limit: %d of %d segments, "
		    "Mailboxes: %d\n", HostAdapter,
		    HostAdapter->DriverScatterGatherLimit,
		    HostAdapter->HostAdapterScatterGatherLimit,
		    HostAdapter->MailboxCount);
      BusLogic_Info("  Driver Queue Depth: %d, "
		    "Host Adapter Queue Depth: %d\n",
		    HostAdapter, HostAdapter->DriverQueueDepth,
		    HostAdapter->HostAdapterQueueDepth);
    }
  else BusLogic_Info("  Driver Queue Depth: %d, "
		     "Scatter/Gather Limit: %d segments\n",
		     HostAdapter, HostAdapter->DriverQueueDepth,
		     HostAdapter->DriverScatterGatherLimit);
  BusLogic_Info("  Tagged Queue Depth: ", HostAdapter);
  CommonTaggedQueueDepth = true;
  for (TargetID = 1; TargetID < HostAdapter->MaxTargetDevices; TargetID++)
    if (HostAdapter->QueueDepth[TargetID] != HostAdapter->QueueDepth[0])
      {
	CommonTaggedQueueDepth = false;
	break;
      }
  if (CommonTaggedQueueDepth)
    {
      if (HostAdapter->QueueDepth[0] > 0)
	BusLogic_Info("%d", HostAdapter, HostAdapter->QueueDepth[0]);
      else BusLogic_Info("Automatic", HostAdapter);
    }
  else BusLogic_Info("Individual", HostAdapter);
  BusLogic_Info(", Untagged Queue Depth: %d\n", HostAdapter,
		HostAdapter->UntaggedQueueDepth);
  CommonErrorRecovery = true;
  for (TargetID = 1; TargetID < HostAdapter->MaxTargetDevices; TargetID++)
    if (HostAdapter->ErrorRecoveryStrategy[TargetID] !=
	HostAdapter->ErrorRecoveryStrategy[0])
      {
	CommonErrorRecovery = false;
	break;
      }
  if (CommonErrorRecovery)
    ErrorRecoveryMessage =
      BusLogic_ErrorRecoveryStrategyNames[
	HostAdapter->ErrorRecoveryStrategy[0]];
  else
    {
      for (TargetID = 0; TargetID < HostAdapter->MaxTargetDevices; TargetID++)
	ErrorRecoveryString[TargetID] =
	  BusLogic_ErrorRecoveryStrategyLetters[
	    HostAdapter->ErrorRecoveryStrategy[TargetID]];
      ErrorRecoveryString[HostAdapter->SCSI_ID] = '#';
      ErrorRecoveryString[HostAdapter->MaxTargetDevices] = '\0';
    }
  BusLogic_Info("  Error Recovery Strategy: %s, SCSI Bus Reset: %s\n",
		HostAdapter, ErrorRecoveryMessage,
		(HostAdapter->BusResetEnabled ? "Enabled" : "Disabled"));
  if (HostAdapter->TerminationInfoValid)
    {
      if (HostAdapter->HostWideSCSI)
	BusLogic_Info("  SCSI Bus Termination: %s", HostAdapter,
		      (HostAdapter->LowByteTerminated
		       ? (HostAdapter->HighByteTerminated
			  ? "Both Enabled" : "Low Enabled")
		       : (HostAdapter->HighByteTerminated
			  ? "High Enabled" : "Both Disabled")));
      else BusLogic_Info("  SCSI Bus Termination: %s", HostAdapter,
			 (HostAdapter->LowByteTerminated ?
			  "Enabled" : "Disabled"));
      if (HostAdapter->HostSupportsSCAM)
	BusLogic_Info(", SCAM: %s", HostAdapter,
		      (HostAdapter->SCAM_Enabled
		       ? (HostAdapter->SCAM_Level2
			  ? "Enabled, Level 2" : "Enabled, Level 1")
		       : "Disabled"));
      BusLogic_Info("\n", HostAdapter);
    }
  /*
    Indicate reporting the Host Adapter configuration completed successfully.
  */
  return true;
}


/*
  BusLogic_AcquireResources acquires the system resources necessary to use
  Host Adapter.
*/

static boolean BusLogic_AcquireResources(BusLogic_HostAdapter_T *HostAdapter)
{
  if (HostAdapter->IRQ_Channel == 0)
    {
      BusLogic_Error("NO LEGAL INTERRUPT CHANNEL ASSIGNED - DETACHING\n",
		     HostAdapter);
      return false;
    }
  /*
    Acquire shared access to the IRQ Channel.
  */
  if (request_irq(HostAdapter->IRQ_Channel, BusLogic_InterruptHandler,
		  SA_SHIRQ, HostAdapter->FullModelName, HostAdapter) < 0)
    {
      BusLogic_Error("UNABLE TO ACQUIRE IRQ CHANNEL %d - DETACHING\n",
		     HostAdapter, HostAdapter->IRQ_Channel);
      return false;
    }
  HostAdapter->IRQ_ChannelAcquired = true;
  /*
    Acquire exclusive access to the DMA Channel.
  */
  if (HostAdapter->DMA_Channel > 0)
    {
      if (request_dma(HostAdapter->DMA_Channel,
		      HostAdapter->FullModelName) < 0)
	{
	  BusLogic_Error("UNABLE TO ACQUIRE DMA CHANNEL %d - DETACHING\n",
			 HostAdapter, HostAdapter->DMA_Channel);
	  return false;
	}
      set_dma_mode(HostAdapter->DMA_Channel, DMA_MODE_CASCADE);
      enable_dma(HostAdapter->DMA_Channel);
      HostAdapter->DMA_ChannelAcquired = true;
    }
  /*
    Indicate the System Resource Acquisition completed successfully,
  */
  return true;
}


/*
  BusLogic_ReleaseResources releases any system resources previously acquired
  by BusLogic_AcquireResources.
*/

static void BusLogic_ReleaseResources(BusLogic_HostAdapter_T *HostAdapter)
{
  /*
    Release shared access to the IRQ Channel.
  */
  if (HostAdapter->IRQ_ChannelAcquired)
    free_irq(HostAdapter->IRQ_Channel, HostAdapter);
  /*
    Release exclusive access to the DMA Channel.
  */
  if (HostAdapter->DMA_ChannelAcquired)
    free_dma(HostAdapter->DMA_Channel);
}


/*
  BusLogic_InitializeHostAdapter initializes Host Adapter.  This is the only
  function called during SCSI Host Adapter detection which modifies the state
  of the Host Adapter from its initial power on or hard reset state.
*/

static boolean BusLogic_InitializeHostAdapter(BusLogic_HostAdapter_T
					      *HostAdapter)
{
  BusLogic_ExtendedMailboxRequest_T ExtendedMailboxRequest;
  BusLogic_RoundRobinModeRequest_T RoundRobinModeRequest;
  BusLogic_SetCCBFormatRequest_T SetCCBFormatRequest;
  int TargetID;
  /*
    Initialize the pointers to the first and last CCBs that are queued for
    completion processing.
  */
  HostAdapter->FirstCompletedCCB = NULL;
  HostAdapter->LastCompletedCCB = NULL;
  /*
    Initialize the Bus Device Reset Pending CCB, Tagged Queuing Active,
    Command Successful Flag, Active Commands, and Commands Since Reset
    for each Target Device.
  */
  for (TargetID = 0; TargetID < HostAdapter->MaxTargetDevices; TargetID++)
    {
      HostAdapter->BusDeviceResetPendingCCB[TargetID] = NULL;
      HostAdapter->TargetFlags[TargetID].TaggedQueuingActive = false;
      HostAdapter->TargetFlags[TargetID].CommandSuccessfulFlag = false;
      HostAdapter->ActiveCommands[TargetID] = 0;
      HostAdapter->CommandsSinceReset[TargetID] = 0;
    }
  /*
    FlashPoint Host Adapters do not use Outgoing and Incoming Mailboxes.
  */
  if (BusLogic_FlashPointHostAdapterP(HostAdapter)) goto Done;
  /*
    Initialize the Outgoing and Incoming Mailbox pointers.
  */
  HostAdapter->FirstOutgoingMailbox =
    (BusLogic_OutgoingMailbox_T *) HostAdapter->MailboxSpace;
  HostAdapter->LastOutgoingMailbox =
    HostAdapter->FirstOutgoingMailbox + HostAdapter->MailboxCount - 1;
  HostAdapter->NextOutgoingMailbox = HostAdapter->FirstOutgoingMailbox;
  HostAdapter->FirstIncomingMailbox =
    (BusLogic_IncomingMailbox_T *) (HostAdapter->LastOutgoingMailbox + 1);
  HostAdapter->LastIncomingMailbox =
    HostAdapter->FirstIncomingMailbox + HostAdapter->MailboxCount - 1;
  HostAdapter->NextIncomingMailbox = HostAdapter->FirstIncomingMailbox;
  /*
    Initialize the Outgoing and Incoming Mailbox structures.
  */
  memset(HostAdapter->FirstOutgoingMailbox, 0,
	 HostAdapter->MailboxCount * sizeof(BusLogic_OutgoingMailbox_T));
  memset(HostAdapter->FirstIncomingMailbox, 0,
	 HostAdapter->MailboxCount * sizeof(BusLogic_IncomingMailbox_T));
  /*
    Initialize the Host Adapter's Pointer to the Outgoing/Incoming Mailboxes.
  */
  ExtendedMailboxRequest.MailboxCount = HostAdapter->MailboxCount;
  ExtendedMailboxRequest.BaseMailboxAddress =
    Virtual_to_Bus(HostAdapter->FirstOutgoingMailbox);
  if (BusLogic_Command(HostAdapter, BusLogic_InitializeExtendedMailbox,
		       &ExtendedMailboxRequest,
		       sizeof(ExtendedMailboxRequest), NULL, 0) < 0)
    return BusLogic_Failure(HostAdapter, "MAILBOX INITIALIZATION");
  /*
    Enable Strict Round Robin Mode if supported by the Host Adapter.  In
    Strict Round Robin Mode, the Host Adapter only looks at the next Outgoing
    Mailbox for each new command, rather than scanning through all the
    Outgoing Mailboxes to find any that have new commands in them.  Strict
    Round Robin Mode is significantly more efficient.
  */
  if (HostAdapter->StrictRoundRobinModeSupport)
    {
      RoundRobinModeRequest = BusLogic_StrictRoundRobinMode;
      if (BusLogic_Command(HostAdapter, BusLogic_EnableStrictRoundRobinMode,
			   &RoundRobinModeRequest,
			   sizeof(RoundRobinModeRequest), NULL, 0) < 0)
	return BusLogic_Failure(HostAdapter, "ENABLE STRICT ROUND ROBIN MODE");
    }
  /*
    For Host Adapters that support Extended LUN Format CCBs, issue the Set CCB
    Format command to allow 32 Logical Units per Target Device.
  */
  if (HostAdapter->ExtendedLUNSupport)
    {
      SetCCBFormatRequest = BusLogic_ExtendedLUNFormatCCB;
      if (BusLogic_Command(HostAdapter, BusLogic_SetCCBFormat,
			   &SetCCBFormatRequest, sizeof(SetCCBFormatRequest),
			   NULL, 0) < 0)
	return BusLogic_Failure(HostAdapter, "SET CCB FORMAT");
    }
  /*
    Announce Successful Initialization.
  */
Done:
  if (!HostAdapter->HostAdapterInitialized)
    {
      BusLogic_Info("*** %s Initialized Successfully ***\n",
		    HostAdapter, HostAdapter->FullModelName);
      BusLogic_Info("\n", HostAdapter);
    }
  else BusLogic_Warning("*** %s Initialized Successfully ***\n",
			HostAdapter, HostAdapter->FullModelName);
  HostAdapter->HostAdapterInitialized = true;
  /*
    Indicate the Host Adapter Initialization completed successfully.
  */
  return true;
}


/*
  BusLogic_TargetDeviceInquiry inquires about the Target Devices accessible
  through Host Adapter.
*/

static boolean BusLogic_TargetDeviceInquiry(BusLogic_HostAdapter_T
					    *HostAdapter)
{
  BusLogic_InstalledDevices_T InstalledDevices;
  BusLogic_InstalledDevices8_T InstalledDevicesID0to7;
  BusLogic_SetupInformation_T SetupInformation;
  BusLogic_SynchronousPeriod_T SynchronousPeriod;
  BusLogic_RequestedReplyLength_T RequestedReplyLength;
  int TargetID;
  /*
    Wait a few seconds between the Host Adapter Hard Reset which initiates
    a SCSI Bus Reset and issuing any SCSI Commands.  Some SCSI devices get
    confused if they receive SCSI Commands too soon after a SCSI Bus Reset.
  */
  BusLogic_Delay(HostAdapter->BusSettleTime);
  /*
    FlashPoint Host Adapters do not provide for Target Device Inquiry.
  */
  if (BusLogic_FlashPointHostAdapterP(HostAdapter)) return true;
  /*
    Inhibit the Target Device Inquiry if requested.
  */
  if (HostAdapter->DriverOptions != NULL &&
      HostAdapter->DriverOptions->LocalOptions.InhibitTargetInquiry)
    return true;
  /*
    Issue the Inquire Target Devices command for host adapters with firmware
    version 4.25 or later, or the Inquire Installed Devices ID 0 to 7 command
    for older host adapters.  This is necessary to force Synchronous Transfer
    Negotiation so that the Inquire Setup Information and Inquire Synchronous
    Period commands will return valid data.  The Inquire Target Devices command
    is preferable to Inquire Installed Devices ID 0 to 7 since it only probes
    Logical Unit 0 of each Target Device.
  */
  if (strcmp(HostAdapter->FirmwareVersion, "4.25") >= 0)
    {
      if (BusLogic_Command(HostAdapter, BusLogic_InquireTargetDevices, NULL, 0,
			   &InstalledDevices, sizeof(InstalledDevices))
	  != sizeof(InstalledDevices))
	return BusLogic_Failure(HostAdapter, "INQUIRE TARGET DEVICES");
      for (TargetID = 0; TargetID < HostAdapter->MaxTargetDevices; TargetID++)
	HostAdapter->TargetFlags[TargetID].TargetExists =
	  (InstalledDevices & (1 << TargetID) ? true : false);
    }
  else
    {
      if (BusLogic_Command(HostAdapter, BusLogic_InquireInstalledDevicesID0to7,
			   NULL, 0, &InstalledDevicesID0to7,
			   sizeof(InstalledDevicesID0to7))
	  != sizeof(InstalledDevicesID0to7))
	return BusLogic_Failure(HostAdapter,
				"INQUIRE INSTALLED DEVICES ID 0 TO 7");
      for (TargetID = 0; TargetID < 8; TargetID++)
	HostAdapter->TargetFlags[TargetID].TargetExists =
	  (InstalledDevicesID0to7[TargetID] != 0 ? true : false);
    }
  /*
    Issue the Inquire Setup Information command.
  */
  RequestedReplyLength = sizeof(SetupInformation);
  if (BusLogic_Command(HostAdapter, BusLogic_InquireSetupInformation,
		       &RequestedReplyLength, sizeof(RequestedReplyLength),
		       &SetupInformation, sizeof(SetupInformation))
      != sizeof(SetupInformation))
    return BusLogic_Failure(HostAdapter, "INQUIRE SETUP INFORMATION");
  for (TargetID = 0; TargetID < HostAdapter->MaxTargetDevices; TargetID++)
      HostAdapter->SynchronousOffset[TargetID] =
	(TargetID < 8
	 ? SetupInformation.SynchronousValuesID0to7[TargetID].Offset
	 : SetupInformation.SynchronousValuesID8to15[TargetID-8].Offset);
  if (strcmp(HostAdapter->FirmwareVersion, "5.06L") >= 0)
    for (TargetID = 0; TargetID < HostAdapter->MaxTargetDevices; TargetID++)
      HostAdapter->TargetFlags[TargetID].WideTransfersActive =
	(TargetID < 8
	 ? (SetupInformation.WideTransfersActiveID0to7 & (1 << TargetID)
	    ? true : false)
	 : (SetupInformation.WideTransfersActiveID8to15 & (1 << (TargetID-8))
	    ? true : false));
  /*
    Issue the Inquire Synchronous Period command.
  */
  if (HostAdapter->FirmwareVersion[0] >= '3')
    {
      RequestedReplyLength = sizeof(SynchronousPeriod);
      if (BusLogic_Command(HostAdapter, BusLogic_InquireSynchronousPeriod,
			   &RequestedReplyLength, sizeof(RequestedReplyLength),
			   &SynchronousPeriod, sizeof(SynchronousPeriod))
	  != sizeof(SynchronousPeriod))
	return BusLogic_Failure(HostAdapter, "INQUIRE SYNCHRONOUS PERIOD");
      for (TargetID = 0; TargetID < HostAdapter->MaxTargetDevices; TargetID++)
	HostAdapter->SynchronousPeriod[TargetID] = SynchronousPeriod[TargetID];
    }
  else
    for (TargetID = 0; TargetID < HostAdapter->MaxTargetDevices; TargetID++)
      if (SetupInformation.SynchronousValuesID0to7[TargetID].Offset > 0)
	HostAdapter->SynchronousPeriod[TargetID] =
	  20 + 5 * SetupInformation.SynchronousValuesID0to7[TargetID]
				   .TransferPeriod;
  /*
    Indicate the Target Device Inquiry completed successfully.
  */
  return true;
}


/*
  BusLogic_ReportTargetDeviceInfo reports about the Target Devices accessible
  through Host Adapter.
*/

static void BusLogic_ReportTargetDeviceInfo(BusLogic_HostAdapter_T
					    *HostAdapter)
{
  int TargetID;
  /*
    Inhibit the Target Device Inquiry and Reporting if requested.
  */
  if (BusLogic_MultiMasterHostAdapterP(HostAdapter) &&
      HostAdapter->DriverOptions != NULL &&
      HostAdapter->DriverOptions->LocalOptions.InhibitTargetInquiry)
    return;
  /*
    Report on the Target Devices found.
  */
  for (TargetID = 0; TargetID < HostAdapter->MaxTargetDevices; TargetID++)
    {
      BusLogic_TargetFlags_T *TargetFlags = &HostAdapter->TargetFlags[TargetID];
      if (TargetFlags->TargetExists && !TargetFlags->TargetInfoReported)
	{
	  int SynchronousTransferRate = 0;
	  if (BusLogic_FlashPointHostAdapterP(HostAdapter))
	    {
	      unsigned char WideTransfersActive;
	      FlashPoint_InquireTargetInfo(
		HostAdapter->CardHandle, TargetID,
		&HostAdapter->SynchronousPeriod[TargetID],
		&HostAdapter->SynchronousOffset[TargetID],
		&WideTransfersActive);
	      TargetFlags->WideTransfersActive = WideTransfersActive;
	    }
	  else if (TargetFlags->WideTransfersSupported &&
		   (HostAdapter->WidePermitted & (1 << TargetID)) &&
		   strcmp(HostAdapter->FirmwareVersion, "5.06L") < 0)
	    TargetFlags->WideTransfersActive = true;
	  if (HostAdapter->SynchronousPeriod[TargetID] > 0)
	    SynchronousTransferRate =
	      100000 / HostAdapter->SynchronousPeriod[TargetID];
	  if (TargetFlags->WideTransfersActive)
	    SynchronousTransferRate <<= 1;
	  if (SynchronousTransferRate >= 9950)
	    {
	      SynchronousTransferRate = (SynchronousTransferRate + 50) / 100;
	      BusLogic_Info("Target %d: Queue Depth %d, %sSynchronous at "
			    "%d.%01d MB/sec, offset %d\n",
			    HostAdapter, TargetID,
			    HostAdapter->QueueDepth[TargetID],
			    (TargetFlags->WideTransfersActive ? "Wide " : ""),
			    SynchronousTransferRate / 10,
			    SynchronousTransferRate % 10,
			    HostAdapter->SynchronousOffset[TargetID]);
	    }
	  else if (SynchronousTransferRate > 0)
	    {
	      SynchronousTransferRate = (SynchronousTransferRate + 5) / 10;
	      BusLogic_Info("Target %d: Queue Depth %d, %sSynchronous at "
			    "%d.%02d MB/sec, offset %d\n",
			    HostAdapter, TargetID,
			    HostAdapter->QueueDepth[TargetID],
			    (TargetFlags->WideTransfersActive ? "Wide " : ""),
			    SynchronousTransferRate / 100,
			    SynchronousTransferRate % 100,
			    HostAdapter->SynchronousOffset[TargetID]);
	    }
	  else BusLogic_Info("Target %d: Queue Depth %d, Asynchronous\n",
			     HostAdapter, TargetID,
			     HostAdapter->QueueDepth[TargetID]);
	  TargetFlags->TargetInfoReported = true;
	}
    }
}


/*
  BusLogic_InitializeHostStructure initializes the fields in the SCSI Host
  structure.  The base, io_port, n_io_ports, irq, and dma_channel fields in the
  SCSI Host structure are intentionally left uninitialized, as this driver
  handles acquisition and release of these resources explicitly, as well as
  ensuring exclusive access to the Host Adapter hardware and data structures
  through explicit acquisition and release of the Host Adapter's Lock.
*/

static void BusLogic_InitializeHostStructure(BusLogic_HostAdapter_T
					       *HostAdapter,
					     SCSI_Host_T *Host)
{
  Host->max_id = HostAdapter->MaxTargetDevices;
  Host->max_lun = HostAdapter->MaxLogicalUnits;
  Host->max_channel = 0;
  Host->unique_id = HostAdapter->IO_Address;
  Host->this_id = HostAdapter->SCSI_ID;
  Host->can_queue = HostAdapter->DriverQueueDepth;
  Host->sg_tablesize = HostAdapter->DriverScatterGatherLimit;
  Host->unchecked_isa_dma = HostAdapter->BounceBuffersRequired;
  Host->cmd_per_lun = HostAdapter->UntaggedQueueDepth;
}


/*
  BusLogic_SelectQueueDepths selects Queue Depths for each Target Device based
  on the Host Adapter's Total Queue Depth and the number, type, speed, and
  capabilities of the Target Devices.  When called for the last Host Adapter,
  it reports on the Target Device Information for all BusLogic Host Adapters
  since all the Target Devices have now been probed.
*/

static void BusLogic_SelectQueueDepths(SCSI_Host_T *Host,
				       SCSI_Device_T *DeviceList)
{
  BusLogic_HostAdapter_T *HostAdapter =
    (BusLogic_HostAdapter_T *) Host->hostdata;
  int TaggedDeviceCount = 0, AutomaticTaggedDeviceCount = 0;
  int UntaggedDeviceCount = 0, AutomaticTaggedQueueDepth = 0;
  int AllocatedQueueDepth = 0;
  SCSI_Device_T *Device;
  int TargetID;
  for (TargetID = 0; TargetID < HostAdapter->MaxTargetDevices; TargetID++)
    if (HostAdapter->TargetFlags[TargetID].TargetExists)
      {
	int QueueDepth = HostAdapter->QueueDepth[TargetID];
	if (HostAdapter->TargetFlags[TargetID].TaggedQueuingSupported &&
	    (HostAdapter->TaggedQueuingPermitted & (1 << TargetID)))
	  {
	    TaggedDeviceCount++;
	    if (QueueDepth == 0) AutomaticTaggedDeviceCount++;
	  }
	else
	  {
	    UntaggedDeviceCount++;
	    if (QueueDepth == 0 ||
		QueueDepth > HostAdapter->UntaggedQueueDepth)
	      {
		QueueDepth = HostAdapter->UntaggedQueueDepth;
		HostAdapter->QueueDepth[TargetID] = QueueDepth;
	      }
	  }
	AllocatedQueueDepth += QueueDepth;
	if (QueueDepth == 1)
	  HostAdapter->TaggedQueuingPermitted &= ~(1 << TargetID);
      }
  HostAdapter->TargetDeviceCount = TaggedDeviceCount + UntaggedDeviceCount;
  if (AutomaticTaggedDeviceCount > 0)
    {
      AutomaticTaggedQueueDepth =
	(HostAdapter->HostAdapterQueueDepth - AllocatedQueueDepth)
	/ AutomaticTaggedDeviceCount;
      if (AutomaticTaggedQueueDepth > BusLogic_MaxAutomaticTaggedQueueDepth)
	AutomaticTaggedQueueDepth = BusLogic_MaxAutomaticTaggedQueueDepth;
      if (AutomaticTaggedQueueDepth < BusLogic_MinAutomaticTaggedQueueDepth)
	AutomaticTaggedQueueDepth = BusLogic_MinAutomaticTaggedQueueDepth;
      for (TargetID = 0; TargetID < HostAdapter->MaxTargetDevices; TargetID++)
	if (HostAdapter->TargetFlags[TargetID].TargetExists &&
	    HostAdapter->QueueDepth[TargetID] == 0)
	  {
	    AllocatedQueueDepth += AutomaticTaggedQueueDepth;
	    HostAdapter->QueueDepth[TargetID] = AutomaticTaggedQueueDepth;
	  }
    }
  for (Device = DeviceList; Device != NULL; Device = Device->next)
    if (Device->host == Host)
      Device->queue_depth = HostAdapter->QueueDepth[Device->id];
  /* Allocate an extra CCB for each Target Device for a Bus Device Reset. */
  AllocatedQueueDepth += HostAdapter->TargetDeviceCount;
  if (AllocatedQueueDepth > HostAdapter->DriverQueueDepth)
    AllocatedQueueDepth = HostAdapter->DriverQueueDepth;
  BusLogic_CreateAdditionalCCBs(HostAdapter,
				AllocatedQueueDepth
				- HostAdapter->AllocatedCCBs,
				false);
  if (HostAdapter == BusLogic_LastRegisteredHostAdapter)
    for (HostAdapter = BusLogic_FirstRegisteredHostAdapter;
	 HostAdapter != NULL;
	 HostAdapter = HostAdapter->Next)
      BusLogic_ReportTargetDeviceInfo(HostAdapter);
}


/*
  BusLogic_DetectHostAdapter probes for BusLogic Host Adapters at the standard
  I/O Addresses where they may be located, initializing, registering, and
  reporting the configuration of each BusLogic Host Adapter it finds.  It
  returns the number of BusLogic Host Adapters successfully initialized and
  registered.
*/

int BusLogic_DetectHostAdapter(SCSI_Host_Template_T *HostTemplate)
{
  int BusLogicHostAdapterCount = 0, DriverOptionsIndex = 0, ProbeIndex;
  BusLogic_HostAdapter_T *PrototypeHostAdapter;
  if (BusLogic_ProbeOptions.NoProbe) return 0;
  BusLogic_ProbeInfoList = (BusLogic_ProbeInfo_T *)
    kmalloc(BusLogic_MaxHostAdapters * sizeof(BusLogic_ProbeInfo_T),
	    GFP_ATOMIC);
  if (BusLogic_ProbeInfoList == NULL)
    {
      BusLogic_Error("BusLogic: Unable to allocate Probe Info List\n", NULL);
      return 0;
    }
  memset(BusLogic_ProbeInfoList, 0,
	 BusLogic_MaxHostAdapters * sizeof(BusLogic_ProbeInfo_T));
  PrototypeHostAdapter = (BusLogic_HostAdapter_T *)
    kmalloc(sizeof(BusLogic_HostAdapter_T), GFP_ATOMIC);
  if (PrototypeHostAdapter == NULL)
    {
      kfree(BusLogic_ProbeInfoList);
      BusLogic_Error("BusLogic: Unable to allocate Prototype "
		     "Host Adapter\n", NULL);
      return 0;
    }
  memset(PrototypeHostAdapter, 0, sizeof(BusLogic_HostAdapter_T));
#ifdef MODULE
  if (BusLogic != NULL)
    BusLogic_Setup(BusLogic);
#endif
  BusLogic_InitializeProbeInfoList(PrototypeHostAdapter);
  for (ProbeIndex = 0; ProbeIndex < BusLogic_ProbeInfoCount; ProbeIndex++)
    {
      BusLogic_ProbeInfo_T *ProbeInfo = &BusLogic_ProbeInfoList[ProbeIndex];
      BusLogic_HostAdapter_T *HostAdapter = PrototypeHostAdapter;
      SCSI_Host_T *Host;
      if (ProbeInfo->IO_Address == 0) continue;
      memset(HostAdapter, 0, sizeof(BusLogic_HostAdapter_T));
      HostAdapter->HostAdapterType = ProbeInfo->HostAdapterType;
      HostAdapter->HostAdapterBusType = ProbeInfo->HostAdapterBusType;
      HostAdapter->IO_Address = ProbeInfo->IO_Address;
      HostAdapter->PCI_Address = ProbeInfo->PCI_Address;
      HostAdapter->Bus = ProbeInfo->Bus;
      HostAdapter->Device = ProbeInfo->Device;
      HostAdapter->IRQ_Channel = ProbeInfo->IRQ_Channel;
      HostAdapter->AddressCount =
	BusLogic_HostAdapterAddressCount[HostAdapter->HostAdapterType];
      /*
	Probe the Host Adapter.  If unsuccessful, abort further initialization.
      */
      if (!BusLogic_ProbeHostAdapter(HostAdapter)) continue;
      /*
	Hard Reset the Host Adapter.  If unsuccessful, abort further
	initialization.
      */
      if (!BusLogic_HardwareResetHostAdapter(HostAdapter, true)) continue;
      /*
	Check the Host Adapter.  If unsuccessful, abort further initialization.
      */
      if (!BusLogic_CheckHostAdapter(HostAdapter)) continue;
      /*
	Initialize the Driver Options field if provided.
      */
      if (DriverOptionsIndex < BusLogic_DriverOptionsCount)
	HostAdapter->DriverOptions =
	  &BusLogic_DriverOptions[DriverOptionsIndex++];
      /*
	Announce the Driver Version and Date, Author's Name, Copyright Notice,
	and Electronic Mail Address.
      */
      BusLogic_AnnounceDriver(HostAdapter);
      /*
	Register usage of the I/O Address range.  From this point onward, any
	failure will be assumed to be due to a problem with the Host Adapter,
	rather than due to having mistakenly identified this port as belonging
	to a BusLogic Host Adapter.  The I/O Address range will not be
	released, thereby preventing it from being incorrectly identified as
	any other type of Host Adapter.
      */
      request_region(HostAdapter->IO_Address, HostAdapter->AddressCount,
		     "BusLogic");
      /*
	Register the SCSI Host structure.
      */
      Host = scsi_register(HostTemplate, sizeof(BusLogic_HostAdapter_T));
      if(Host==NULL)
      {
      	release_region(HostAdapter->IO_Address, HostAdapter->AddressCount);
      	continue;
      }
      HostAdapter = (BusLogic_HostAdapter_T *) Host->hostdata;
      memcpy(HostAdapter, PrototypeHostAdapter, sizeof(BusLogic_HostAdapter_T));
      HostAdapter->SCSI_Host = Host;
      HostAdapter->HostNumber = Host->host_no;
      Host->select_queue_depths = BusLogic_SelectQueueDepths;
      /*
	Add Host Adapter to the end of the list of registered BusLogic
	Host Adapters.
      */
      BusLogic_RegisterHostAdapter(HostAdapter);
      /*
	Read the Host Adapter Configuration, Configure the Host Adapter,
	Acquire the System Resources necessary to use the Host Adapter, then
	Create the Initial CCBs, Initialize the Host Adapter, and finally
	perform Target Device Inquiry.
      */
      if (BusLogic_ReadHostAdapterConfiguration(HostAdapter) &&
	  BusLogic_ReportHostAdapterConfiguration(HostAdapter) &&
	  BusLogic_AcquireResources(HostAdapter) &&
	  BusLogic_CreateInitialCCBs(HostAdapter) &&
	  BusLogic_InitializeHostAdapter(HostAdapter) &&
	  BusLogic_TargetDeviceInquiry(HostAdapter))
	{
	  /*
	    Initialization has been completed successfully.  Release and
	    re-register usage of the I/O Address range so that the Model
	    Name of the Host Adapter will appear, and initialize the SCSI
	    Host structure.
	  */
	  release_region(HostAdapter->IO_Address,
			 HostAdapter->AddressCount);
	  request_region(HostAdapter->IO_Address,
			 HostAdapter->AddressCount,
			 HostAdapter->FullModelName);
	  BusLogic_InitializeHostStructure(HostAdapter, Host);
	  BusLogicHostAdapterCount++;
	}
      else
	{
	  /*
	    An error occurred during Host Adapter Configuration Querying, Host
	    Adapter Configuration, Resource Acquisition, CCB Creation, Host
	    Adapter Initialization, or Target Device Inquiry, so remove Host
	    Adapter from the list of registered BusLogic Host Adapters, destroy
	    the CCBs, Release the System Resources, and Unregister the SCSI
	    Host.
	  */
	  BusLogic_DestroyCCBs(HostAdapter);
	  BusLogic_ReleaseResources(HostAdapter);
	  BusLogic_UnregisterHostAdapter(HostAdapter);
	  scsi_unregister(Host);
	}
    }
  kfree(PrototypeHostAdapter);
  kfree(BusLogic_ProbeInfoList);
  BusLogic_ProbeInfoList = NULL;
  return BusLogicHostAdapterCount;
}


/*
  BusLogic_ReleaseHostAdapter releases all resources previously acquired to
  support a specific Host Adapter, including the I/O Address range, and
  unregisters the BusLogic Host Adapter.
*/

int BusLogic_ReleaseHostAdapter(SCSI_Host_T *Host)
{
  BusLogic_HostAdapter_T *HostAdapter =
    (BusLogic_HostAdapter_T *) Host->hostdata;
  /*
    FlashPoint Host Adapters must first be released by the FlashPoint
    SCCB Manager.
  */
  if (BusLogic_FlashPointHostAdapterP(HostAdapter))
    FlashPoint_ReleaseHostAdapter(HostAdapter->CardHandle);
  /*
    Destroy the CCBs and release any system resources acquired to
    support Host Adapter.
  */
  BusLogic_DestroyCCBs(HostAdapter);
  BusLogic_ReleaseResources(HostAdapter);
  /*
    Release usage of the I/O Address range.
  */
  release_region(HostAdapter->IO_Address, HostAdapter->AddressCount);
  /*
    Remove Host Adapter from the list of registered BusLogic Host Adapters.
  */
  BusLogic_UnregisterHostAdapter(HostAdapter);
  return 0;
}


/*
  BusLogic_QueueCompletedCCB queues CCB for completion processing.
*/

static void BusLogic_QueueCompletedCCB(BusLogic_CCB_T *CCB)
{
  BusLogic_HostAdapter_T *HostAdapter = CCB->HostAdapter;
  CCB->Status = BusLogic_CCB_Completed;
  CCB->Next = NULL;
  if (HostAdapter->FirstCompletedCCB == NULL)
    {
      HostAdapter->FirstCompletedCCB = CCB;
      HostAdapter->LastCompletedCCB = CCB;
    }
  else
    {
      HostAdapter->LastCompletedCCB->Next = CCB;
      HostAdapter->LastCompletedCCB = CCB;
    }
  HostAdapter->ActiveCommands[CCB->TargetID]--;
}


/*
  BusLogic_ComputeResultCode computes a SCSI Subsystem Result Code from
  the Host Adapter Status and Target Device Status.
*/

static int BusLogic_ComputeResultCode(BusLogic_HostAdapter_T *HostAdapter,
				      BusLogic_HostAdapterStatus_T
					HostAdapterStatus,
				      BusLogic_TargetDeviceStatus_T
					TargetDeviceStatus)
{
  int HostStatus;
  switch (HostAdapterStatus)
    {
    case BusLogic_CommandCompletedNormally:
    case BusLogic_LinkedCommandCompleted:
    case BusLogic_LinkedCommandCompletedWithFlag:
      HostStatus = DID_OK;
      break;
    case BusLogic_SCSISelectionTimeout:
      HostStatus = DID_TIME_OUT;
      break;
    case BusLogic_InvalidOutgoingMailboxActionCode:
    case BusLogic_InvalidCommandOperationCode:
    case BusLogic_InvalidCommandParameter:
      BusLogic_Warning("BusLogic Driver Protocol Error 0x%02X\n",
		       HostAdapter, HostAdapterStatus);
    case BusLogic_DataUnderRun:
    case BusLogic_DataOverRun:
    case BusLogic_UnexpectedBusFree:
    case BusLogic_LinkedCCBhasInvalidLUN:
    case BusLogic_AutoRequestSenseFailed:
    case BusLogic_TaggedQueuingMessageRejected:
    case BusLogic_UnsupportedMessageReceived:
    case BusLogic_HostAdapterHardwareFailed:
    case BusLogic_TargetDeviceReconnectedImproperly:
    case BusLogic_AbortQueueGenerated:
    case BusLogic_HostAdapterSoftwareError:
    case BusLogic_HostAdapterHardwareTimeoutError:
    case BusLogic_SCSIParityErrorDetected:
      HostStatus = DID_ERROR;
      break;
    case BusLogic_InvalidBusPhaseRequested:
    case BusLogic_TargetFailedResponseToATN:
    case BusLogic_HostAdapterAssertedRST:
    case BusLogic_OtherDeviceAssertedRST:
    case BusLogic_HostAdapterAssertedBusDeviceReset:
      HostStatus = DID_RESET;
      break;
    default:
      BusLogic_Warning("Unknown Host Adapter Status 0x%02X\n",
		       HostAdapter, HostAdapterStatus);
      HostStatus = DID_ERROR;
      break;
    }
  return (HostStatus << 16) | TargetDeviceStatus;
}


/*
  BusLogic_ScanIncomingMailboxes scans the Incoming Mailboxes saving any
  Incoming Mailbox entries for completion processing.
*/

static void BusLogic_ScanIncomingMailboxes(BusLogic_HostAdapter_T *HostAdapter)
{
  /*
    Scan through the Incoming Mailboxes in Strict Round Robin fashion, saving
    any completed CCBs for further processing.  It is essential that for each
    CCB and SCSI Command issued, command completion processing is performed
    exactly once.  Therefore, only Incoming Mailboxes with completion code
    Command Completed Without Error, Command Completed With Error, or Command
    Aborted At Host Request are saved for completion processing.  When an
    Incoming Mailbox has a completion code of Aborted Command Not Found, the
    CCB had already completed or been aborted before the current Abort request
    was processed, and so completion processing has already occurred and no
    further action should be taken.
  */
  BusLogic_IncomingMailbox_T *NextIncomingMailbox =
    HostAdapter->NextIncomingMailbox;
  BusLogic_CompletionCode_T CompletionCode;
  while ((CompletionCode = NextIncomingMailbox->CompletionCode) !=
	 BusLogic_IncomingMailboxFree)
    {
      BusLogic_CCB_T *CCB = (BusLogic_CCB_T *)
	Bus_to_Virtual(NextIncomingMailbox->CCB);
      if (CompletionCode != BusLogic_AbortedCommandNotFound)
	{
	  if (CCB->Status == BusLogic_CCB_Active ||
	      CCB->Status == BusLogic_CCB_Reset)
	    {
	      /*
		Save the Completion Code for this CCB and queue the CCB
		for completion processing.
	      */
	      CCB->CompletionCode = CompletionCode;
	      BusLogic_QueueCompletedCCB(CCB);
	    }
	  else
	    {
	      /*
		If a CCB ever appears in an Incoming Mailbox and is not marked
		as status Active or Reset, then there is most likely a bug in
		the Host Adapter firmware.
	      */
	      BusLogic_Warning("Illegal CCB #%ld status %d in "
			       "Incoming Mailbox\n", HostAdapter,
			       CCB->SerialNumber, CCB->Status);
	    }
	}
      NextIncomingMailbox->CompletionCode = BusLogic_IncomingMailboxFree;
      if (++NextIncomingMailbox > HostAdapter->LastIncomingMailbox)
	NextIncomingMailbox = HostAdapter->FirstIncomingMailbox;
    }
  HostAdapter->NextIncomingMailbox = NextIncomingMailbox;
}


/*
  BusLogic_ProcessCompletedCCBs iterates over the completed CCBs for Host
  Adapter setting the SCSI Command Result Codes, deallocating the CCBs, and
  calling the SCSI Subsystem Completion Routines.  The Host Adapter's Lock
  should already have been acquired by the caller.
*/

static void BusLogic_ProcessCompletedCCBs(BusLogic_HostAdapter_T *HostAdapter)
{
  if (HostAdapter->ProcessCompletedCCBsActive) return;
  HostAdapter->ProcessCompletedCCBsActive = true;
  while (HostAdapter->FirstCompletedCCB != NULL)
    {
      BusLogic_CCB_T *CCB = HostAdapter->FirstCompletedCCB;
      SCSI_Command_T *Command = CCB->Command;
      HostAdapter->FirstCompletedCCB = CCB->Next;
      if (HostAdapter->FirstCompletedCCB == NULL)
	HostAdapter->LastCompletedCCB = NULL;
      /*
	Process the Completed CCB.
      */
      if (CCB->Opcode == BusLogic_BusDeviceReset)
	{
	  int TargetID = CCB->TargetID;
	  BusLogic_Warning("Bus Device Reset CCB #%ld to Target "
			   "%d Completed\n", HostAdapter,
			   CCB->SerialNumber, TargetID);
	  BusLogic_IncrementErrorCounter(
	    &HostAdapter->TargetStatistics[TargetID].BusDeviceResetsCompleted);
	  HostAdapter->TargetFlags[TargetID].TaggedQueuingActive = false;
	  HostAdapter->CommandsSinceReset[TargetID] = 0;
	  HostAdapter->LastResetCompleted[TargetID] = jiffies;
	  /*
	    Place CCB back on the Host Adapter's free list.
	  */
	  BusLogic_DeallocateCCB(CCB);
	  /*
	    Bus Device Reset CCBs have the Command field non-NULL only when a
	    Bus Device Reset was requested for a Command that did not have a
	    currently active CCB in the Host Adapter (i.e., a Synchronous
	    Bus Device Reset), and hence would not have its Completion Routine
	    called otherwise.
	  */
	  while (Command != NULL)
	    {
	      SCSI_Command_T *NextCommand = Command->reset_chain;
	      Command->reset_chain = NULL;
	      Command->result = DID_RESET << 16;
	      Command->scsi_done(Command);
	      Command = NextCommand;
	    }
	  /*
	    Iterate over the CCBs for this Host Adapter performing completion
	    processing for any CCBs marked as Reset for this Target.
	  */
	  for (CCB = HostAdapter->All_CCBs; CCB != NULL; CCB = CCB->NextAll)
	    if (CCB->Status == BusLogic_CCB_Reset && CCB->TargetID == TargetID)
	      {
		Command = CCB->Command;
		BusLogic_DeallocateCCB(CCB);
		HostAdapter->ActiveCommands[TargetID]--;
		Command->result = DID_RESET << 16;
		Command->scsi_done(Command);
	      }
	  HostAdapter->BusDeviceResetPendingCCB[TargetID] = NULL;
	}
      else
	{
	  /*
	    Translate the Completion Code, Host Adapter Status, and Target
	    Device Status into a SCSI Subsystem Result Code.
	  */
	  switch (CCB->CompletionCode)
	    {
	    case BusLogic_IncomingMailboxFree:
	    case BusLogic_AbortedCommandNotFound:
	    case BusLogic_InvalidCCB:
	      BusLogic_Warning("CCB #%ld to Target %d Impossible State\n",
			       HostAdapter, CCB->SerialNumber, CCB->TargetID);
	      break;
	    case BusLogic_CommandCompletedWithoutError:
	      HostAdapter->TargetStatistics[CCB->TargetID]
			   .CommandsCompleted++;
	      HostAdapter->TargetFlags[CCB->TargetID]
			   .CommandSuccessfulFlag = true;
	      Command->result = DID_OK << 16;
	      break;
	    case BusLogic_CommandAbortedAtHostRequest:
	      BusLogic_Warning("CCB #%ld to Target %d Aborted\n",
			       HostAdapter, CCB->SerialNumber, CCB->TargetID);
	      BusLogic_IncrementErrorCounter(
		&HostAdapter->TargetStatistics[CCB->TargetID]
			      .CommandAbortsCompleted);
	      Command->result = DID_ABORT << 16;
	      break;
	    case BusLogic_CommandCompletedWithError:
	      Command->result =
		BusLogic_ComputeResultCode(HostAdapter,
					   CCB->HostAdapterStatus,
					   CCB->TargetDeviceStatus);
	      if (CCB->HostAdapterStatus != BusLogic_SCSISelectionTimeout)
		{
		  HostAdapter->TargetStatistics[CCB->TargetID]
			       .CommandsCompleted++;
		  if (BusLogic_GlobalOptions.TraceErrors)
		    {
		      int i;
		      BusLogic_Notice("CCB #%ld Target %d: Result %X Host "
				      "Adapter Status %02X "
				      "Target Status %02X\n",
				      HostAdapter, CCB->SerialNumber,
				      CCB->TargetID, Command->result,
				      CCB->HostAdapterStatus,
				      CCB->TargetDeviceStatus);
		      BusLogic_Notice("CDB   ", HostAdapter);
		      for (i = 0; i < CCB->CDB_Length; i++)
			BusLogic_Notice(" %02X", HostAdapter, CCB->CDB[i]);
		      BusLogic_Notice("\n", HostAdapter);
		      BusLogic_Notice("Sense ", HostAdapter);
		      for (i = 0; i < CCB->SenseDataLength; i++)
			BusLogic_Notice(" %02X", HostAdapter,
					Command->sense_buffer[i]);
		      BusLogic_Notice("\n", HostAdapter);
		    }
		}
	      break;
	    }
	  /*
	    When an INQUIRY command completes normally, save the
	    CmdQue (Tagged Queuing Supported) and WBus16 (16 Bit
	    Wide Data Transfers Supported) bits.
	  */
	  if (CCB->CDB[0] == INQUIRY && CCB->CDB[1] == 0 &&
	      CCB->HostAdapterStatus == BusLogic_CommandCompletedNormally)
	    {
	      BusLogic_TargetFlags_T *TargetFlags =
		&HostAdapter->TargetFlags[CCB->TargetID];
	      SCSI_Inquiry_T *InquiryResult =
		(SCSI_Inquiry_T *) Command->request_buffer;
	      TargetFlags->TargetExists = true;
	      TargetFlags->TaggedQueuingSupported = InquiryResult->CmdQue;
	      TargetFlags->WideTransfersSupported = InquiryResult->WBus16;
	    }
	  /*
	    Place CCB back on the Host Adapter's free list.
	  */
	  BusLogic_DeallocateCCB(CCB);
	  /*
	    Call the SCSI Command Completion Routine.
	  */
	  Command->scsi_done(Command);
	}
    }
  HostAdapter->ProcessCompletedCCBsActive = false;
}


/*
  BusLogic_InterruptHandler handles hardware interrupts from BusLogic Host
  Adapters.
*/

static void BusLogic_InterruptHandler(int IRQ_Channel,
				      void *DeviceIdentifier,
				      Registers_T *InterruptRegisters)
{
  BusLogic_HostAdapter_T *HostAdapter =
    (BusLogic_HostAdapter_T *) DeviceIdentifier;
  ProcessorFlags_T ProcessorFlags;
  /*
    Acquire exclusive access to Host Adapter.
  */
  BusLogic_AcquireHostAdapterLockIH(HostAdapter, &ProcessorFlags);
  /*
    Handle Interrupts appropriately for each Host Adapter type.
  */
  if (BusLogic_MultiMasterHostAdapterP(HostAdapter))
    {
      BusLogic_InterruptRegister_T InterruptRegister;
      /*
	Read the Host Adapter Interrupt Register.
      */
      InterruptRegister.All = BusLogic_ReadInterruptRegister(HostAdapter);
      if (InterruptRegister.Bits.InterruptValid)
	{
	  /*
	    Acknowledge the interrupt and reset the Host Adapter
	    Interrupt Register.
	  */
	  BusLogic_InterruptReset(HostAdapter);
	  /*
	    Process valid External SCSI Bus Reset and Incoming Mailbox
	    Loaded Interrupts.  Command Complete Interrupts are noted,
	    and Outgoing Mailbox Available Interrupts are ignored, as
	    they are never enabled.
	  */
	  if (InterruptRegister.Bits.ExternalBusReset)
	    HostAdapter->HostAdapterExternalReset = true;
	  else if (InterruptRegister.Bits.IncomingMailboxLoaded)
	    BusLogic_ScanIncomingMailboxes(HostAdapter);
	  else if (InterruptRegister.Bits.CommandComplete)
	    HostAdapter->HostAdapterCommandCompleted = true;
	}
    }
  else
    {
      /*
	Check if there is a pending interrupt for this Host Adapter.
      */
      if (FlashPoint_InterruptPending(HostAdapter->CardHandle))
	switch (FlashPoint_HandleInterrupt(HostAdapter->CardHandle))
	  {
	  case FlashPoint_NormalInterrupt:
	    break;
	  case FlashPoint_ExternalBusReset:
	    HostAdapter->HostAdapterExternalReset = true;
	    break;
	  case FlashPoint_InternalError:
	    BusLogic_Warning("Internal FlashPoint Error detected"
			     " - Resetting Host Adapter\n", HostAdapter);
	    HostAdapter->HostAdapterInternalError = true;
	    break;
	  }
    }
  /*
    Process any completed CCBs.
  */
  if (HostAdapter->FirstCompletedCCB != NULL)
    BusLogic_ProcessCompletedCCBs(HostAdapter);
  /*
    Reset the Host Adapter if requested.
  */
  if (HostAdapter->HostAdapterExternalReset ||
      HostAdapter->HostAdapterInternalError)
    {
      BusLogic_ResetHostAdapter(HostAdapter, NULL, 0);
      HostAdapter->HostAdapterExternalReset = false;
      HostAdapter->HostAdapterInternalError = false;
      scsi_mark_host_reset(HostAdapter->SCSI_Host);
    }
  /*
    Release exclusive access to Host Adapter.
  */
  BusLogic_ReleaseHostAdapterLockIH(HostAdapter, &ProcessorFlags);
}


/*
  BusLogic_WriteOutgoingMailbox places CCB and Action Code into an Outgoing
  Mailbox for execution by Host Adapter.  The Host Adapter's Lock should
  already have been acquired by the caller.
*/

static boolean BusLogic_WriteOutgoingMailbox(BusLogic_HostAdapter_T
					       *HostAdapter,
					     BusLogic_ActionCode_T ActionCode,
					     BusLogic_CCB_T *CCB)
{
  BusLogic_OutgoingMailbox_T *NextOutgoingMailbox;
  NextOutgoingMailbox = HostAdapter->NextOutgoingMailbox;
  if (NextOutgoingMailbox->ActionCode == BusLogic_OutgoingMailboxFree)
    {
      CCB->Status = BusLogic_CCB_Active;
      /*
	The CCB field must be written before the Action Code field since
	the Host Adapter is operating asynchronously and the locking code
	does not protect against simultaneous access by the Host Adapter.
      */
      NextOutgoingMailbox->CCB = Virtual_to_Bus(CCB);
      NextOutgoingMailbox->ActionCode = ActionCode;
      BusLogic_StartMailboxCommand(HostAdapter);
      if (++NextOutgoingMailbox > HostAdapter->LastOutgoingMailbox)
	NextOutgoingMailbox = HostAdapter->FirstOutgoingMailbox;
      HostAdapter->NextOutgoingMailbox = NextOutgoingMailbox;
      if (ActionCode == BusLogic_MailboxStartCommand)
	{
	  HostAdapter->ActiveCommands[CCB->TargetID]++;
	  if (CCB->Opcode != BusLogic_BusDeviceReset)
	    HostAdapter->TargetStatistics[CCB->TargetID].CommandsAttempted++;
	}
      return true;
    }
  return false;
}


/*
  BusLogic_QueueCommand creates a CCB for Command and places it into an
  Outgoing Mailbox for execution by the associated Host Adapter.
*/

int BusLogic_QueueCommand(SCSI_Command_T *Command,
			  void (*CompletionRoutine)(SCSI_Command_T *))
{
  BusLogic_HostAdapter_T *HostAdapter =
    (BusLogic_HostAdapter_T *) Command->host->hostdata;
  BusLogic_TargetFlags_T *TargetFlags =
    &HostAdapter->TargetFlags[Command->target];
  BusLogic_TargetStatistics_T *TargetStatistics =
    HostAdapter->TargetStatistics;
  unsigned char *CDB = Command->cmnd;
  int CDB_Length = Command->cmd_len;
  int TargetID = Command->target;
  int LogicalUnit = Command->lun;
  void *BufferPointer = Command->request_buffer;
  int BufferLength = Command->request_bufflen;
  int SegmentCount = Command->use_sg;
  ProcessorFlags_T ProcessorFlags;
  BusLogic_CCB_T *CCB;
  /*
    SCSI REQUEST_SENSE commands will be executed automatically by the Host
    Adapter for any errors, so they should not be executed explicitly unless
    the Sense Data is zero indicating that no error occurred.
  */
  if (CDB[0] == REQUEST_SENSE && Command->sense_buffer[0] != 0)
    {
      Command->result = DID_OK << 16;
      CompletionRoutine(Command);
      return 0;
    }
  /*
    Acquire exclusive access to Host Adapter.
  */
  BusLogic_AcquireHostAdapterLock(HostAdapter, &ProcessorFlags);
  /*
    Allocate a CCB from the Host Adapter's free list.  In the unlikely event
    that there are none available and memory allocation fails, wait 1 second
    and try again.  If that fails, the Host Adapter is probably hung so signal
    an error as a Host Adapter Hard Reset should be initiated soon.
  */
  CCB = BusLogic_AllocateCCB(HostAdapter);
  if (CCB == NULL)
    {
      BusLogic_Delay(1);
      CCB = BusLogic_AllocateCCB(HostAdapter);
      if (CCB == NULL)
	{
	  Command->result = DID_ERROR << 16;
	  CompletionRoutine(Command);
	  goto Done;
	}
    }
  /*
    Initialize the fields in the BusLogic Command Control Block (CCB).
  */
  if (SegmentCount == 0)
    {
      CCB->Opcode = BusLogic_InitiatorCCB;
      CCB->DataLength = BufferLength;
      CCB->DataPointer = Virtual_to_Bus(BufferPointer);
    }
  else
    {
      SCSI_ScatterList_T *ScatterList = (SCSI_ScatterList_T *) BufferPointer;
      int Segment;
      CCB->Opcode = BusLogic_InitiatorCCB_ScatterGather;
      CCB->DataLength = SegmentCount * sizeof(BusLogic_ScatterGatherSegment_T);
      if (BusLogic_MultiMasterHostAdapterP(HostAdapter))
	CCB->DataPointer = Virtual_to_Bus(CCB->ScatterGatherList);
      else CCB->DataPointer = Virtual_to_32Bit_Virtual(CCB->ScatterGatherList);
      for (Segment = 0; Segment < SegmentCount; Segment++)
	{
	  CCB->ScatterGatherList[Segment].SegmentByteCount =
	    ScatterList[Segment].length;
	  CCB->ScatterGatherList[Segment].SegmentDataPointer =
	    Virtual_to_Bus(ScatterList[Segment].address);
	}
    }
  switch (CDB[0])
    {
    case READ_6:
    case READ_10:
      CCB->DataDirection = BusLogic_DataInLengthChecked;
      TargetStatistics[TargetID].ReadCommands++;
      BusLogic_IncrementByteCounter(
	&TargetStatistics[TargetID].TotalBytesRead, BufferLength);
      BusLogic_IncrementSizeBucket(
	TargetStatistics[TargetID].ReadCommandSizeBuckets, BufferLength);
      break;
    case WRITE_6:
    case WRITE_10:
      CCB->DataDirection = BusLogic_DataOutLengthChecked;
      TargetStatistics[TargetID].WriteCommands++;
      BusLogic_IncrementByteCounter(
	&TargetStatistics[TargetID].TotalBytesWritten, BufferLength);
      BusLogic_IncrementSizeBucket(
	TargetStatistics[TargetID].WriteCommandSizeBuckets, BufferLength);
      break;
    default:
      CCB->DataDirection = BusLogic_UncheckedDataTransfer;
      break;
    }
  CCB->CDB_Length = CDB_Length;
  CCB->SenseDataLength = sizeof(Command->sense_buffer);
  CCB->HostAdapterStatus = 0;
  CCB->TargetDeviceStatus = 0;
  CCB->TargetID = TargetID;
  CCB->LogicalUnit = LogicalUnit;
  CCB->TagEnable = false;
  CCB->LegacyTagEnable = false;
  /*
    BusLogic recommends that after a Reset the first couple of commands that
    are sent to a Target Device be sent in a non Tagged Queue fashion so that
    the Host Adapter and Target Device can establish Synchronous and Wide
    Transfer before Queue Tag messages can interfere with the Synchronous and
    Wide Negotiation messages.  By waiting to enable Tagged Queuing until after
    the first BusLogic_MaxTaggedQueueDepth commands have been queued, it is
    assured that after a Reset any pending commands are requeued before Tagged
    Queuing is enabled and that the Tagged Queuing message will not occur while
    the partition table is being printed.  In addition, some devices do not
    properly handle the transition from non-tagged to tagged commands, so it is
    necessary to wait until there are no pending commands for a target device
    before queuing tagged commands.
  */
  if (HostAdapter->CommandsSinceReset[TargetID]++ >=
	BusLogic_MaxTaggedQueueDepth &&
      !TargetFlags->TaggedQueuingActive &&
      HostAdapter->ActiveCommands[TargetID] == 0 &&
      TargetFlags->TaggedQueuingSupported &&
      (HostAdapter->TaggedQueuingPermitted & (1 << TargetID)))
    {
      TargetFlags->TaggedQueuingActive = true;
      BusLogic_Notice("Tagged Queuing now active for Target %d\n",
		      HostAdapter, TargetID);
    }
  if (TargetFlags->TaggedQueuingActive)
    {
      BusLogic_QueueTag_T QueueTag = BusLogic_SimpleQueueTag;
      /*
	When using Tagged Queuing with Simple Queue Tags, it appears that disk
	drive controllers do not guarantee that a queued command will not
	remain in a disconnected state indefinitely if commands that read or
	write nearer the head position continue to arrive without interruption.
	Therefore, for each Target Device this driver keeps track of the last
	time either the queue was empty or an Ordered Queue Tag was issued.  If
	more than 4 seconds (one fifth of the 20 second disk timeout) have
	elapsed since this last sequence point, this command will be issued
	with an Ordered Queue Tag rather than a Simple Queue Tag, which forces
	the Target Device to complete all previously queued commands before
	this command may be executed.
      */
      if (HostAdapter->ActiveCommands[TargetID] == 0)
	HostAdapter->LastSequencePoint[TargetID] = jiffies;
      else if (jiffies - HostAdapter->LastSequencePoint[TargetID] > 4*HZ)
	{
	  HostAdapter->LastSequencePoint[TargetID] = jiffies;
	  QueueTag = BusLogic_OrderedQueueTag;
	}
      if (HostAdapter->ExtendedLUNSupport)
	{
	  CCB->TagEnable = true;
	  CCB->QueueTag = QueueTag;
	}
      else
	{
	  CCB->LegacyTagEnable = true;
	  CCB->LegacyQueueTag = QueueTag;
	}
    }
  memcpy(CCB->CDB, CDB, CDB_Length);
  CCB->SenseDataPointer = Virtual_to_Bus(&Command->sense_buffer);
  CCB->Command = Command;
  Command->scsi_done = CompletionRoutine;
  if (BusLogic_MultiMasterHostAdapterP(HostAdapter))
    {
      /*
	Place the CCB in an Outgoing Mailbox.  The higher levels of the SCSI
	Subsystem should not attempt to queue more commands than can be placed
	in Outgoing Mailboxes, so there should always be one free.  In the
	unlikely event that there are none available, wait 1 second and try
	again.  If that fails, the Host Adapter is probably hung so signal an
	error as a Host Adapter Hard Reset should be initiated soon.
      */
      if (!BusLogic_WriteOutgoingMailbox(
	     HostAdapter, BusLogic_MailboxStartCommand, CCB))
	{
	  BusLogic_Warning("Unable to write Outgoing Mailbox - "
			   "Pausing for 1 second\n", HostAdapter);
	  BusLogic_Delay(1);
	  if (!BusLogic_WriteOutgoingMailbox(
		 HostAdapter, BusLogic_MailboxStartCommand, CCB))
	    {
	      BusLogic_Warning("Still unable to write Outgoing Mailbox - "
			       "Host Adapter Dead?\n", HostAdapter);
	      BusLogic_DeallocateCCB(CCB);
	      Command->result = DID_ERROR << 16;
	      Command->scsi_done(Command);
	    }
	}
    }
  else
    {
      /*
	Call the FlashPoint SCCB Manager to start execution of the CCB.
      */
      CCB->Status = BusLogic_CCB_Active;
      HostAdapter->ActiveCommands[TargetID]++;
      TargetStatistics[TargetID].CommandsAttempted++;
      FlashPoint_StartCCB(HostAdapter->CardHandle, CCB);
      /*
	The Command may have already completed and BusLogic_QueueCompletedCCB
	been called, or it may still be pending.
      */
      if (CCB->Status == BusLogic_CCB_Completed)
	BusLogic_ProcessCompletedCCBs(HostAdapter);
    }
  /*
    Release exclusive access to Host Adapter.
  */
Done:
  BusLogic_ReleaseHostAdapterLock(HostAdapter, &ProcessorFlags);
  return 0;
}


/*
  BusLogic_AbortCommand aborts Command if possible.
*/

int BusLogic_AbortCommand(SCSI_Command_T *Command)
{
  BusLogic_HostAdapter_T *HostAdapter =
    (BusLogic_HostAdapter_T *) Command->host->hostdata;
  int TargetID = Command->target;
  ProcessorFlags_T ProcessorFlags;
  BusLogic_CCB_T *CCB;
  int Result;
  BusLogic_IncrementErrorCounter(
    &HostAdapter->TargetStatistics[TargetID].CommandAbortsRequested);
  /*
    Acquire exclusive access to Host Adapter.
  */
  BusLogic_AcquireHostAdapterLock(HostAdapter, &ProcessorFlags);
  /*
    If this Command has already completed, then no Abort is necessary.
  */
  if (Command->serial_number != Command->serial_number_at_timeout)
    {
      BusLogic_Warning("Unable to Abort Command to Target %d - "
		       "Already Completed\n", HostAdapter, TargetID);
      Result = SCSI_ABORT_NOT_RUNNING;
      goto Done;
    }
  /*
    Attempt to find an Active CCB for this Command.  If no Active CCB for this
    Command is found, then no Abort is necessary.
  */
  for (CCB = HostAdapter->All_CCBs; CCB != NULL; CCB = CCB->NextAll)
    if (CCB->Command == Command) break;
  if (CCB == NULL)
    {
      BusLogic_Warning("Unable to Abort Command to Target %d - "
		       "No CCB Found\n", HostAdapter, TargetID);
      Result = SCSI_ABORT_NOT_RUNNING;
      goto Done;
    }
  else if (CCB->Status == BusLogic_CCB_Completed)
    {
      BusLogic_Warning("Unable to Abort Command to Target %d - "
		       "CCB Completed\n", HostAdapter, TargetID);
      Result = SCSI_ABORT_NOT_RUNNING;
      goto Done;
    }
  else if (CCB->Status == BusLogic_CCB_Reset)
    {
      BusLogic_Warning("Unable to Abort Command to Target %d - "
		       "CCB Reset\n", HostAdapter, TargetID);
      Result = SCSI_ABORT_PENDING;
      goto Done;
    }
  if (BusLogic_MultiMasterHostAdapterP(HostAdapter))
    {
      /*
	Attempt to Abort this CCB.  MultiMaster Firmware versions prior to 5.xx
	do not generate Abort Tag messages, but only generate the non-tagged
	Abort message.  Since non-tagged commands are not sent by the Host
	Adapter until the queue of outstanding tagged commands has completed,
	and the Abort message is treated as a non-tagged command, it is
	effectively impossible to abort commands when Tagged Queuing is active.
	Firmware version 5.xx does generate Abort Tag messages, so it is
	possible to abort commands when Tagged Queuing is active.
      */
      if (HostAdapter->TargetFlags[TargetID].TaggedQueuingActive &&
	  HostAdapter->FirmwareVersion[0] < '5')
	{
	  BusLogic_Warning("Unable to Abort CCB #%ld to Target %d - "
			   "Abort Tag Not Supported\n",
			   HostAdapter, CCB->SerialNumber, TargetID);
	  Result = SCSI_ABORT_SNOOZE;
	}
      else if (BusLogic_WriteOutgoingMailbox(
		 HostAdapter, BusLogic_MailboxAbortCommand, CCB))
	{
	  BusLogic_Warning("Aborting CCB #%ld to Target %d\n",
			   HostAdapter, CCB->SerialNumber, TargetID);
	  BusLogic_IncrementErrorCounter(
	    &HostAdapter->TargetStatistics[TargetID].CommandAbortsAttempted);
	  Result = SCSI_ABORT_PENDING;
	}
      else
	{
	  BusLogic_Warning("Unable to Abort CCB #%ld to Target %d - "
			   "No Outgoing Mailboxes\n",
			    HostAdapter, CCB->SerialNumber, TargetID);
	  Result = SCSI_ABORT_BUSY;
	}
    }
  else
    {
      /*
	Call the FlashPoint SCCB Manager to abort execution of the CCB.
      */
      BusLogic_Warning("Aborting CCB #%ld to Target %d\n",
		       HostAdapter, CCB->SerialNumber, TargetID);
      BusLogic_IncrementErrorCounter(
	&HostAdapter->TargetStatistics[TargetID].CommandAbortsAttempted);
      FlashPoint_AbortCCB(HostAdapter->CardHandle, CCB);
      /*
	The Abort may have already been completed and
	BusLogic_QueueCompletedCCB been called, or it
	may still be pending.
      */
      Result = SCSI_ABORT_PENDING;
      if (CCB->Status == BusLogic_CCB_Completed)
	{
	  BusLogic_ProcessCompletedCCBs(HostAdapter);
	  Result = SCSI_ABORT_SUCCESS;
	}
    }
  /*
    Release exclusive access to Host Adapter.
  */
Done:
  BusLogic_ReleaseHostAdapterLock(HostAdapter, &ProcessorFlags);
  return Result;
}


/*
  BusLogic_ResetHostAdapter resets Host Adapter if possible, marking all
  currently executing SCSI Commands as having been Reset.
*/

static int BusLogic_ResetHostAdapter(BusLogic_HostAdapter_T *HostAdapter,
				     SCSI_Command_T *Command,
				     unsigned int ResetFlags)
{
  ProcessorFlags_T ProcessorFlags;
  BusLogic_CCB_T *CCB;
  int TargetID, Result;
  boolean HardReset;
  if (HostAdapter->HostAdapterExternalReset)
    {
      BusLogic_IncrementErrorCounter(&HostAdapter->ExternalHostAdapterResets);
      HardReset = false;
    }
  else if (HostAdapter->HostAdapterInternalError)
    {
      BusLogic_IncrementErrorCounter(&HostAdapter->HostAdapterInternalErrors);
      HardReset = true;
    }
  else
    {
      BusLogic_IncrementErrorCounter(
	&HostAdapter->TargetStatistics[Command->target]
		      .HostAdapterResetsRequested);
      HardReset = true;
    }
  /*
    Acquire exclusive access to Host Adapter.
  */
  BusLogic_AcquireHostAdapterLock(HostAdapter, &ProcessorFlags);
  /*
    If this is an Asynchronous Reset and this Command has already completed,
    then no Reset is necessary.
  */
  if (ResetFlags & SCSI_RESET_ASYNCHRONOUS)
    {
      TargetID = Command->target;
      if (Command->serial_number != Command->serial_number_at_timeout)
	{
	  BusLogic_Warning("Unable to Reset Command to Target %d - "
			   "Already Completed or Reset\n",
			   HostAdapter, TargetID);
	  Result = SCSI_RESET_NOT_RUNNING;
	  goto Done;
      }
      for (CCB = HostAdapter->All_CCBs; CCB != NULL; CCB = CCB->NextAll)
	if (CCB->Command == Command) break;
      if (CCB == NULL)
	{
	  BusLogic_Warning("Unable to Reset Command to Target %d - "
			   "No CCB Found\n", HostAdapter, TargetID);
	  Result = SCSI_RESET_NOT_RUNNING;
	  goto Done;
	}
      else if (CCB->Status == BusLogic_CCB_Completed)
	{
	  BusLogic_Warning("Unable to Reset Command to Target %d - "
			   "CCB Completed\n", HostAdapter, TargetID);
	  Result = SCSI_RESET_NOT_RUNNING;
	  goto Done;
	}
      else if (CCB->Status == BusLogic_CCB_Reset &&
	       HostAdapter->BusDeviceResetPendingCCB[TargetID] == NULL)
	{
	  BusLogic_Warning("Unable to Reset Command to Target %d - "
			   "Reset Pending\n", HostAdapter, TargetID);
	  Result = SCSI_RESET_PENDING;
	  goto Done;
	}
    }
  if (Command == NULL)
    {
      if (HostAdapter->HostAdapterInternalError)
	BusLogic_Warning("Resetting %s due to Host Adapter Internal Error\n",
			 HostAdapter, HostAdapter->FullModelName);
      else BusLogic_Warning("Resetting %s due to External SCSI Bus Reset\n",
			    HostAdapter, HostAdapter->FullModelName);
    }
  else
    {
      BusLogic_Warning("Resetting %s due to Target %d\n", HostAdapter,
		       HostAdapter->FullModelName, Command->target);
      BusLogic_IncrementErrorCounter(
	&HostAdapter->TargetStatistics[Command->target]
		      .HostAdapterResetsAttempted);
    }
  /*
    Attempt to Reset and Reinitialize the Host Adapter.
  */
  if (!(BusLogic_HardwareResetHostAdapter(HostAdapter, HardReset) &&
	BusLogic_InitializeHostAdapter(HostAdapter)))
    {
      BusLogic_Error("Resetting %s Failed\n", HostAdapter,
		     HostAdapter->FullModelName);
      Result = SCSI_RESET_ERROR;
      goto Done;
    }
  if (Command != NULL)
    BusLogic_IncrementErrorCounter(
      &HostAdapter->TargetStatistics[Command->target]
		    .HostAdapterResetsCompleted);
  /*
    Mark all currently executing CCBs as having been Reset.
  */
  for (CCB = HostAdapter->All_CCBs; CCB != NULL; CCB = CCB->NextAll)
    if (CCB->Status == BusLogic_CCB_Active)
      CCB->Status = BusLogic_CCB_Reset;
  /*
    Wait a few seconds between the Host Adapter Hard Reset which initiates
    a SCSI Bus Reset and issuing any SCSI Commands.  Some SCSI devices get
    confused if they receive SCSI Commands too soon after a SCSI Bus Reset.
    Note that a timer interrupt may occur here, but all active CCBs have
    already been marked Reset and so a reentrant call will return Pending.
  */
  if (HardReset)
    BusLogic_Delay(HostAdapter->BusSettleTime);
  /*
    If this is a Synchronous Reset, perform completion processing for
    the Command being Reset.
  */
  if (ResetFlags & SCSI_RESET_SYNCHRONOUS)
    {
      Command->result = DID_RESET << 16;
      Command->scsi_done(Command);
    }
  /*
    Perform completion processing for all CCBs marked as Reset.
  */
  for (CCB = HostAdapter->All_CCBs; CCB != NULL; CCB = CCB->NextAll)
    if (CCB->Status == BusLogic_CCB_Reset)
      {
	Command = CCB->Command;
	BusLogic_DeallocateCCB(CCB);
	while (Command != NULL)
	  {
	    SCSI_Command_T *NextCommand = Command->reset_chain;
	    Command->reset_chain = NULL;
	    Command->result = DID_RESET << 16;
	    Command->scsi_done(Command);
	    Command = NextCommand;
	  }
      }
  for (TargetID = 0; TargetID < HostAdapter->MaxTargetDevices; TargetID++)
    {
      HostAdapter->LastResetAttempted[TargetID] = jiffies;
      HostAdapter->LastResetCompleted[TargetID] = jiffies;
    }
  Result = SCSI_RESET_SUCCESS | SCSI_RESET_HOST_RESET;
  /*
    Release exclusive access to Host Adapter.
  */
Done:
  BusLogic_ReleaseHostAdapterLock(HostAdapter, &ProcessorFlags);
  return Result;
}


/*
  BusLogic_SendBusDeviceReset sends a Bus Device Reset to the Target
  Device associated with Command.
*/

static int BusLogic_SendBusDeviceReset(BusLogic_HostAdapter_T *HostAdapter,
				       SCSI_Command_T *Command,
				       unsigned int ResetFlags)
{
  int TargetID = Command->target;
  BusLogic_CCB_T *CCB, *XCCB;
  ProcessorFlags_T ProcessorFlags;
  int Result = -1;
  BusLogic_IncrementErrorCounter(
    &HostAdapter->TargetStatistics[TargetID].BusDeviceResetsRequested);
  /*
    Acquire exclusive access to Host Adapter.
  */
  BusLogic_AcquireHostAdapterLock(HostAdapter, &ProcessorFlags);
  /*
    If this is an Asynchronous Reset and this Command has already completed,
    then no Reset is necessary.
  */
  if (ResetFlags & SCSI_RESET_ASYNCHRONOUS)
    {
      if (Command->serial_number != Command->serial_number_at_timeout)
	{
	  BusLogic_Warning("Unable to Reset Command to Target %d - "
			   "Already Completed\n", HostAdapter, TargetID);
	  Result = SCSI_RESET_NOT_RUNNING;
	  goto Done;
	}
      for (CCB = HostAdapter->All_CCBs; CCB != NULL; CCB = CCB->NextAll)
	if (CCB->Command == Command) break;
      if (CCB == NULL)
	{
	  BusLogic_Warning("Unable to Reset Command to Target %d - "
			   "No CCB Found\n", HostAdapter, TargetID);
	  Result = SCSI_RESET_NOT_RUNNING;
	  goto Done;
	}
      else if (CCB->Status == BusLogic_CCB_Completed)
	{
	  BusLogic_Warning("Unable to Reset Command to Target %d - "
			   "CCB Completed\n", HostAdapter, TargetID);
	  Result = SCSI_RESET_NOT_RUNNING;
	  goto Done;
	}
      else if (CCB->Status == BusLogic_CCB_Reset)
	{
	  BusLogic_Warning("Unable to Reset Command to Target %d - "
			   "Reset Pending\n", HostAdapter, TargetID);
	  Result = SCSI_RESET_PENDING;
	  goto Done;
	}
      else if (HostAdapter->BusDeviceResetPendingCCB[TargetID] != NULL)
	{
	  BusLogic_Warning("Bus Device Reset already pending to Target %d\n",
			   HostAdapter, TargetID);
	  goto Done;
	}
    }
  /*
    If this is a Synchronous Reset and a Bus Device Reset is already pending
    for this Target Device, do not send a second one.  Add this Command to
    the list of Commands for which completion processing must be performed
    when the Bus Device Reset CCB completes.
  */
  if (ResetFlags & SCSI_RESET_SYNCHRONOUS)
    if ((CCB = HostAdapter->BusDeviceResetPendingCCB[TargetID]) != NULL)
      {
	Command->reset_chain = CCB->Command;
	CCB->Command = Command;
	BusLogic_Warning("Unable to Reset Command to Target %d - "
			 "Reset Pending\n", HostAdapter, TargetID);
	Result = SCSI_RESET_PENDING;
	goto Done;
      }
  if (BusLogic_MultiMasterHostAdapterP(HostAdapter))
    {
      /*
	MultiMaster Firmware versions prior to 5.xx treat a Bus Device Reset as
	a non-tagged command.  Since non-tagged commands are not sent by the
	Host Adapter until the queue of outstanding tagged commands has
	completed, it is effectively impossible to send a Bus Device Reset
	while there are tagged commands outstanding.  Therefore, in that case a
	full Host Adapter Hard Reset and SCSI Bus Reset must be done.
      */
      if (HostAdapter->TargetFlags[TargetID].TaggedQueuingActive &&
	  HostAdapter->ActiveCommands[TargetID] > 0 &&
	  HostAdapter->FirmwareVersion[0] < '5')
	goto Done;
    }
  /*
    Allocate a CCB from the Host Adapter's free list.  In the unlikely event
    that there are none available and memory allocation fails, attempt a full
    Host Adapter Hard Reset and SCSI Bus Reset.
  */
  CCB = BusLogic_AllocateCCB(HostAdapter);
  if (CCB == NULL) goto Done;
  BusLogic_Warning("Sending Bus Device Reset CCB #%ld to Target %d\n",
		   HostAdapter, CCB->SerialNumber, TargetID);
  CCB->Opcode = BusLogic_BusDeviceReset;
  CCB->TargetID = TargetID;
  /*
    For Synchronous Resets, arrange for the interrupt handler to perform
    completion processing for the Command being Reset.
  */
  if (ResetFlags & SCSI_RESET_SYNCHRONOUS)
    {
      Command->reset_chain = NULL;
      CCB->Command = Command;
    }
  if (BusLogic_MultiMasterHostAdapterP(HostAdapter))
    {
      /*
	Attempt to write an Outgoing Mailbox with the Bus Device Reset CCB.
	If sending a Bus Device Reset is impossible, attempt a full Host
	Adapter Hard Reset and SCSI Bus Reset.
      */
      if (!(BusLogic_WriteOutgoingMailbox(
	      HostAdapter, BusLogic_MailboxStartCommand, CCB)))
	{
	  BusLogic_Warning("Unable to write Outgoing Mailbox for "
			   "Bus Device Reset\n", HostAdapter);
	  BusLogic_DeallocateCCB(CCB);
	  goto Done;
	}
    }
  else
    {
      /*
	Call the FlashPoint SCCB Manager to start execution of the CCB.
      */
      CCB->Status = BusLogic_CCB_Active;
      HostAdapter->ActiveCommands[TargetID]++;
      FlashPoint_StartCCB(HostAdapter->CardHandle, CCB);
    }
  /*
    If there is a currently executing CCB in the Host Adapter for this Command
    (i.e. this is an Asynchronous Reset), then an Incoming Mailbox entry may be
    made with a completion code of BusLogic_HostAdapterAssertedBusDeviceReset.
    If there is no active CCB for this Command (i.e. this is a Synchronous
    Reset), then the Bus Device Reset CCB's Command field will have been set
    to the Command so that the interrupt for the completion of the Bus Device
    Reset can call the Completion Routine for the Command.  On successful
    execution of a Bus Device Reset, older firmware versions did return the
    pending CCBs with the appropriate completion code, but more recent firmware
    versions only return the Bus Device Reset CCB itself.  This driver handles
    both cases by marking all the currently executing CCBs to this Target
    Device as Reset.  When the Bus Device Reset CCB is processed by the
    interrupt handler, any remaining CCBs marked as Reset will have completion
    processing performed.
  */
  BusLogic_IncrementErrorCounter(
    &HostAdapter->TargetStatistics[TargetID].BusDeviceResetsAttempted);
  HostAdapter->BusDeviceResetPendingCCB[TargetID] = CCB;
  HostAdapter->LastResetAttempted[TargetID] = jiffies;
  for (XCCB = HostAdapter->All_CCBs; XCCB != NULL; XCCB = XCCB->NextAll)
    if (XCCB->Status == BusLogic_CCB_Active && XCCB->TargetID == TargetID)
      XCCB->Status = BusLogic_CCB_Reset;
  /*
    FlashPoint Host Adapters may have already completed the Bus Device
    Reset and BusLogic_QueueCompletedCCB been called, or it may still be
    pending.
  */
  Result = SCSI_RESET_PENDING;
  if (BusLogic_FlashPointHostAdapterP(HostAdapter))
    if (CCB->Status == BusLogic_CCB_Completed)
      {
	BusLogic_ProcessCompletedCCBs(HostAdapter);
	Result = SCSI_RESET_SUCCESS;
      }
  /*
    If a Bus Device Reset was not possible for some reason, force a full
    Host Adapter Hard Reset and SCSI Bus Reset.
  */
Done:
  if (Result < 0)
    Result = BusLogic_ResetHostAdapter(HostAdapter, Command, ResetFlags);
  /*
    Release exclusive access to Host Adapter.
  */
  BusLogic_ReleaseHostAdapterLock(HostAdapter, &ProcessorFlags);
  return Result;
}


/*
  BusLogic_ResetCommand takes appropriate action to reset Command.
*/

int BusLogic_ResetCommand(SCSI_Command_T *Command, unsigned int ResetFlags)
{
  BusLogic_HostAdapter_T *HostAdapter =
    (BusLogic_HostAdapter_T *) Command->host->hostdata;
  int TargetID = Command->target;
  BusLogic_ErrorRecoveryStrategy_T
    ErrorRecoveryStrategy = HostAdapter->ErrorRecoveryStrategy[TargetID];
  /*
    Disable Tagged Queuing if it is active for this Target Device and if
    it has been less than 10 minutes since the last reset occurred, or since
    the system was initialized if no prior resets have occurred.
  */
  if (HostAdapter->TargetFlags[TargetID].TaggedQueuingActive &&
      jiffies - HostAdapter->LastResetCompleted[TargetID] < 10*60*HZ)
    {
      HostAdapter->TaggedQueuingPermitted &= ~(1 << TargetID);
      HostAdapter->TargetFlags[TargetID].TaggedQueuingActive = false;
      BusLogic_Warning("Tagged Queuing now disabled for Target %d\n",
		       HostAdapter, TargetID);
    }
  switch (ErrorRecoveryStrategy)
    {
    case BusLogic_ErrorRecovery_Default:
      if (ResetFlags & SCSI_RESET_SUGGEST_HOST_RESET)
	return BusLogic_ResetHostAdapter(HostAdapter, Command, ResetFlags);
      else if (ResetFlags & SCSI_RESET_SUGGEST_BUS_RESET)
	return BusLogic_ResetHostAdapter(HostAdapter, Command, ResetFlags);
      /* Fall through to Bus Device Reset case. */
    case BusLogic_ErrorRecovery_BusDeviceReset:
      /*
	The Bus Device Reset Error Recovery Strategy only graduates to a Hard
	Reset when no commands have completed successfully since the last Bus
	Device Reset and it has been at least 100 milliseconds.  This prevents
	a sequence of commands that all timeout together from immediately
	forcing a Hard Reset before the Bus Device Reset has had a chance to
	clear the error condition.
      */
      if (HostAdapter->TargetFlags[TargetID].CommandSuccessfulFlag ||
	  jiffies - HostAdapter->LastResetAttempted[TargetID] < HZ/10)
	{
	  HostAdapter->TargetFlags[TargetID].CommandSuccessfulFlag = false;
	  return BusLogic_SendBusDeviceReset(HostAdapter, Command, ResetFlags);
	}
      /* Fall through to Hard Reset case. */
    case BusLogic_ErrorRecovery_HardReset:
      return BusLogic_ResetHostAdapter(HostAdapter, Command, ResetFlags);
    case BusLogic_ErrorRecovery_None:
      BusLogic_Warning("Error Recovery for Target %d Suppressed\n",
		       HostAdapter, TargetID);
      break;
    }
  return SCSI_RESET_PUNT;
}


/*
  BusLogic_BIOSDiskParameters returns the Heads/Sectors/Cylinders BIOS Disk
  Parameters for Disk.  The default disk geometry is 64 heads, 32 sectors, and
  the appropriate number of cylinders so as not to exceed drive capacity.  In
  order for disks equal to or larger than 1 GB to be addressable by the BIOS
  without exceeding the BIOS limitation of 1024 cylinders, Extended Translation
  may be enabled in AutoSCSI on FlashPoint Host Adapters and on "W" and "C"
  series MultiMaster Host Adapters, or by a dip switch setting on "S" and "A"
  series MultiMaster Host Adapters.  With Extended Translation enabled, drives
  between 1 GB inclusive and 2 GB exclusive are given a disk geometry of 128
  heads and 32 sectors, and drives above 2 GB inclusive are given a disk
  geometry of 255 heads and 63 sectors.  However, if the BIOS detects that the
  Extended Translation setting does not match the geometry in the partition
  table, then the translation inferred from the partition table will be used by
  the BIOS, and a warning may be displayed.
*/

int BusLogic_BIOSDiskParameters(SCSI_Disk_T *Disk, KernelDevice_T Device,
				int *Parameters)
{
  BusLogic_HostAdapter_T *HostAdapter =
    (BusLogic_HostAdapter_T *) Disk->device->host->hostdata;
  BIOS_DiskParameters_T *DiskParameters = (BIOS_DiskParameters_T *) Parameters;
  struct buffer_head *BufferHead;
  if (HostAdapter->ExtendedTranslationEnabled &&
      Disk->capacity >= 2*1024*1024 /* 1 GB in 512 byte sectors */)
    {
      if (Disk->capacity >= 4*1024*1024 /* 2 GB in 512 byte sectors */)
	{
	  DiskParameters->Heads = 255;
	  DiskParameters->Sectors = 63;
	}
      else
	{
	  DiskParameters->Heads = 128;
	  DiskParameters->Sectors = 32;
	}
    }
  else
    {
      DiskParameters->Heads = 64;
      DiskParameters->Sectors = 32;
    }
  DiskParameters->Cylinders =
    Disk->capacity / (DiskParameters->Heads * DiskParameters->Sectors);
  /*
    Attempt to read the first 1024 bytes from the disk device.
  */
  BufferHead = bread(MKDEV(MAJOR(Device), MINOR(Device) & ~0x0F), 0, block_size(Device));
  if (BufferHead == NULL) return 0;
  /*
    If the boot sector partition table flag is valid, search for a partition
    table entry whose end_head matches one of the standard BusLogic geometry
    translations (64/32, 128/32, or 255/63).
  */
  if (*(unsigned short *) (BufferHead->b_data + 0x1FE) == 0xAA55)
    {
      PartitionTable_T *FirstPartitionEntry =
	(PartitionTable_T *) (BufferHead->b_data + 0x1BE);
      PartitionTable_T *PartitionEntry = FirstPartitionEntry;
      int SavedCylinders = DiskParameters->Cylinders, PartitionNumber;
      unsigned char PartitionEntryEndHead, PartitionEntryEndSector;
      for (PartitionNumber = 0; PartitionNumber < 4; PartitionNumber++)
	{
	  PartitionEntryEndHead = PartitionEntry->end_head;
	  PartitionEntryEndSector = PartitionEntry->end_sector & 0x3F;
	  if (PartitionEntryEndHead == 64-1)
	    {
	      DiskParameters->Heads = 64;
	      DiskParameters->Sectors = 32;
	      break;
	    }
	  else if (PartitionEntryEndHead == 128-1)
	    {
	      DiskParameters->Heads = 128;
	      DiskParameters->Sectors = 32;
	      break;
	    }
	  else if (PartitionEntryEndHead == 255-1)
	    {
	      DiskParameters->Heads = 255;
	      DiskParameters->Sectors = 63;
	      break;
	    }
	  PartitionEntry++;
	}
      if (PartitionNumber == 4)
	{
	  PartitionEntryEndHead = FirstPartitionEntry->end_head;
	  PartitionEntryEndSector = FirstPartitionEntry->end_sector & 0x3F;
	}
      DiskParameters->Cylinders =
	Disk->capacity / (DiskParameters->Heads * DiskParameters->Sectors);
      if (PartitionNumber < 4 &&
	  PartitionEntryEndSector == DiskParameters->Sectors)
	{
	  if (DiskParameters->Cylinders != SavedCylinders)
	    BusLogic_Warning("Adopting Geometry %d/%d from Partition Table\n",
			     HostAdapter,
			     DiskParameters->Heads, DiskParameters->Sectors);
	}
      else if (PartitionEntryEndHead > 0 || PartitionEntryEndSector > 0)
	{
	  BusLogic_Warning("Warning: Partition Table appears to "
			   "have Geometry %d/%d which is\n", HostAdapter,
			   PartitionEntryEndHead + 1,
			   PartitionEntryEndSector);
	  BusLogic_Warning("not compatible with current BusLogic "
			   "Host Adapter Geometry %d/%d\n", HostAdapter,
			   DiskParameters->Heads, DiskParameters->Sectors);
	}
    }
  brelse(BufferHead);
  return 0;
}


/*
  BugLogic_ProcDirectoryInfo implements /proc/scsi/BusLogic/<N>.
*/

int BusLogic_ProcDirectoryInfo(char *ProcBuffer, char **StartPointer,
			       off_t Offset, int BytesAvailable,
			       int HostNumber, int WriteFlag)
{
  BusLogic_HostAdapter_T *HostAdapter;
  BusLogic_TargetStatistics_T *TargetStatistics;
  int TargetID, Length;
  char *Buffer;
  for (HostAdapter = BusLogic_FirstRegisteredHostAdapter;
       HostAdapter != NULL;
       HostAdapter = HostAdapter->Next)
    if (HostAdapter->HostNumber == HostNumber) break;
  if (HostAdapter == NULL)
    {
      BusLogic_Error("Cannot find Host Adapter for SCSI Host %d\n",
		     NULL, HostNumber);
      return 0;
    }
  TargetStatistics = HostAdapter->TargetStatistics;
  if (WriteFlag)
    {
      HostAdapter->ExternalHostAdapterResets = 0;
      HostAdapter->HostAdapterInternalErrors = 0;
      memset(TargetStatistics, 0,
	     BusLogic_MaxTargetDevices * sizeof(BusLogic_TargetStatistics_T));
      return 0;
    }
  Buffer = HostAdapter->MessageBuffer;
  Length = HostAdapter->MessageBufferLength;
  Length += sprintf(&Buffer[Length], "\n\
Current Driver Queue Depth:	%d\n\
Currently Allocated CCBs:	%d\n",
		    HostAdapter->DriverQueueDepth,
		    HostAdapter->AllocatedCCBs);
  Length += sprintf(&Buffer[Length], "\n\n\
			   DATA TRANSFER STATISTICS\n\
\n\
Target	Tagged Queuing	Queue Depth  Active  Attempted	Completed\n\
======	==============	===========  ======  =========	=========\n");
  for (TargetID = 0; TargetID < HostAdapter->MaxTargetDevices; TargetID++)
    {
      BusLogic_TargetFlags_T *TargetFlags = &HostAdapter->TargetFlags[TargetID];
      if (!TargetFlags->TargetExists) continue;
      Length +=
	sprintf(&Buffer[Length], "  %2d	%s", TargetID,
		(TargetFlags->TaggedQueuingSupported
		 ? (TargetFlags->TaggedQueuingActive
		    ? "    Active"
		    : (HostAdapter->TaggedQueuingPermitted & (1 << TargetID)
		       ? "  Permitted" : "   Disabled"))
		 : "Not Supported"));
      Length += sprintf(&Buffer[Length],
			"	    %3d       %3u    %9u	%9u\n",
			HostAdapter->QueueDepth[TargetID],
			HostAdapter->ActiveCommands[TargetID],
			TargetStatistics[TargetID].CommandsAttempted,
			TargetStatistics[TargetID].CommandsCompleted);
    }
  Length += sprintf(&Buffer[Length], "\n\
Target  Read Commands  Write Commands   Total Bytes Read    Total Bytes Written\n\
======  =============  ==============  ===================  ===================\n");
  for (TargetID = 0; TargetID < HostAdapter->MaxTargetDevices; TargetID++)
    {
      BusLogic_TargetFlags_T *TargetFlags = &HostAdapter->TargetFlags[TargetID];
      if (!TargetFlags->TargetExists) continue;
      Length +=
	sprintf(&Buffer[Length], "  %2d	  %9u	 %9u", TargetID,
		TargetStatistics[TargetID].ReadCommands,
		TargetStatistics[TargetID].WriteCommands);
      if (TargetStatistics[TargetID].TotalBytesRead.Billions > 0)
	Length +=
	  sprintf(&Buffer[Length], "     %9u%09u",
		  TargetStatistics[TargetID].TotalBytesRead.Billions,
		  TargetStatistics[TargetID].TotalBytesRead.Units);
      else
	Length +=
	  sprintf(&Buffer[Length], "		%9u",
		  TargetStatistics[TargetID].TotalBytesRead.Units);
      if (TargetStatistics[TargetID].TotalBytesWritten.Billions > 0)
	Length +=
	  sprintf(&Buffer[Length], "   %9u%09u\n",
		  TargetStatistics[TargetID].TotalBytesWritten.Billions,
		  TargetStatistics[TargetID].TotalBytesWritten.Units);
      else
	Length +=
	  sprintf(&Buffer[Length], "	     %9u\n",
		  TargetStatistics[TargetID].TotalBytesWritten.Units);
    }
  Length += sprintf(&Buffer[Length], "\n\
Target  Command    0-1KB      1-2KB      2-4KB      4-8KB     8-16KB\n\
======  =======  =========  =========  =========  =========  =========\n");
  for (TargetID = 0; TargetID < HostAdapter->MaxTargetDevices; TargetID++)
    {
      BusLogic_TargetFlags_T *TargetFlags = &HostAdapter->TargetFlags[TargetID];
      if (!TargetFlags->TargetExists) continue;
      Length +=
	sprintf(&Buffer[Length],
		"  %2d	 Read	 %9u  %9u  %9u  %9u  %9u\n", TargetID,
		TargetStatistics[TargetID].ReadCommandSizeBuckets[0],
		TargetStatistics[TargetID].ReadCommandSizeBuckets[1],
		TargetStatistics[TargetID].ReadCommandSizeBuckets[2],
		TargetStatistics[TargetID].ReadCommandSizeBuckets[3],
		TargetStatistics[TargetID].ReadCommandSizeBuckets[4]);
      Length +=
	sprintf(&Buffer[Length],
		"  %2d	 Write	 %9u  %9u  %9u  %9u  %9u\n", TargetID,
		TargetStatistics[TargetID].WriteCommandSizeBuckets[0],
		TargetStatistics[TargetID].WriteCommandSizeBuckets[1],
		TargetStatistics[TargetID].WriteCommandSizeBuckets[2],
		TargetStatistics[TargetID].WriteCommandSizeBuckets[3],
		TargetStatistics[TargetID].WriteCommandSizeBuckets[4]);
    }
  Length += sprintf(&Buffer[Length], "\n\
Target  Command   16-32KB    32-64KB   64-128KB   128-256KB   256KB+\n\
======  =======  =========  =========  =========  =========  =========\n");
  for (TargetID = 0; TargetID < HostAdapter->MaxTargetDevices; TargetID++)
    {
      BusLogic_TargetFlags_T *TargetFlags = &HostAdapter->TargetFlags[TargetID];
      if (!TargetFlags->TargetExists) continue;
      Length +=
	sprintf(&Buffer[Length],
		"  %2d	 Read	 %9u  %9u  %9u  %9u  %9u\n", TargetID,
		TargetStatistics[TargetID].ReadCommandSizeBuckets[5],
		TargetStatistics[TargetID].ReadCommandSizeBuckets[6],
		TargetStatistics[TargetID].ReadCommandSizeBuckets[7],
		TargetStatistics[TargetID].ReadCommandSizeBuckets[8],
		TargetStatistics[TargetID].ReadCommandSizeBuckets[9]);
      Length +=
	sprintf(&Buffer[Length],
		"  %2d	 Write	 %9u  %9u  %9u  %9u  %9u\n", TargetID,
		TargetStatistics[TargetID].WriteCommandSizeBuckets[5],
		TargetStatistics[TargetID].WriteCommandSizeBuckets[6],
		TargetStatistics[TargetID].WriteCommandSizeBuckets[7],
		TargetStatistics[TargetID].WriteCommandSizeBuckets[8],
		TargetStatistics[TargetID].WriteCommandSizeBuckets[9]);
    }
  Length += sprintf(&Buffer[Length], "\n\n\
			   ERROR RECOVERY STATISTICS\n\
\n\
	  Command Aborts      Bus Device Resets	  Host Adapter Resets\n\
Target	Requested Completed  Requested Completed  Requested Completed\n\
  ID	\\\\\\\\ Attempted ////  \\\\\\\\ Attempted ////  \\\\\\\\ Attempted ////\n\
======	 ===== ===== =====    ===== ===== =====	   ===== ===== =====\n");
  for (TargetID = 0; TargetID < HostAdapter->MaxTargetDevices; TargetID++)
    {
      BusLogic_TargetFlags_T *TargetFlags = &HostAdapter->TargetFlags[TargetID];
      if (!TargetFlags->TargetExists) continue;
      Length +=
	sprintf(&Buffer[Length], "\
  %2d	 %5d %5d %5d    %5d %5d %5d	   %5d %5d %5d\n", TargetID,
		TargetStatistics[TargetID].CommandAbortsRequested,
		TargetStatistics[TargetID].CommandAbortsAttempted,
		TargetStatistics[TargetID].CommandAbortsCompleted,
		TargetStatistics[TargetID].BusDeviceResetsRequested,
		TargetStatistics[TargetID].BusDeviceResetsAttempted,
		TargetStatistics[TargetID].BusDeviceResetsCompleted,
		TargetStatistics[TargetID].HostAdapterResetsRequested,
		TargetStatistics[TargetID].HostAdapterResetsAttempted,
		TargetStatistics[TargetID].HostAdapterResetsCompleted);
    }
  Length += sprintf(&Buffer[Length], "\nExternal Host Adapter Resets: %d\n",
		    HostAdapter->ExternalHostAdapterResets);
  Length += sprintf(&Buffer[Length], "Host Adapter Internal Errors: %d\n",
		    HostAdapter->HostAdapterInternalErrors);
  if (Length >= BusLogic_MessageBufferSize)
    BusLogic_Error("Message Buffer length %d exceeds size %d\n",
		   HostAdapter, Length, BusLogic_MessageBufferSize);
  if ((Length -= Offset) <= 0) return 0;
  if (Length >= BytesAvailable) Length = BytesAvailable;
  memcpy(ProcBuffer, HostAdapter->MessageBuffer + Offset, Length);
  *StartPointer = ProcBuffer;
  return Length;
}


/*
  BusLogic_Message prints Driver Messages.
*/

static void BusLogic_Message(BusLogic_MessageLevel_T MessageLevel,
			     char *Format,
			     BusLogic_HostAdapter_T *HostAdapter,
			     ...)
{
  static char Buffer[BusLogic_LineBufferSize];
  static boolean BeginningOfLine = true;
  va_list Arguments;
  int Length = 0;
  va_start(Arguments, HostAdapter);
  Length = vsprintf(Buffer, Format, Arguments);
  va_end(Arguments);
  if (MessageLevel == BusLogic_AnnounceLevel)
    {
      static int AnnouncementLines = 0;
      strcpy(&HostAdapter->MessageBuffer[HostAdapter->MessageBufferLength],
	     Buffer);
      HostAdapter->MessageBufferLength += Length;
      if (++AnnouncementLines <= 2)
	printk("%sscsi: %s", BusLogic_MessageLevelMap[MessageLevel], Buffer);
    }
  else if (MessageLevel == BusLogic_InfoLevel)
    {
      strcpy(&HostAdapter->MessageBuffer[HostAdapter->MessageBufferLength],
	     Buffer);
      HostAdapter->MessageBufferLength += Length;
      if (BeginningOfLine)
	{
	  if (Buffer[0] != '\n' || Length > 1)
	    printk("%sscsi%d: %s", BusLogic_MessageLevelMap[MessageLevel],
		   HostAdapter->HostNumber, Buffer);
	}
      else printk("%s", Buffer);
    }
  else
    {
      if (BeginningOfLine)
	{
	  if (HostAdapter != NULL && HostAdapter->HostAdapterInitialized)
	    printk("%sscsi%d: %s", BusLogic_MessageLevelMap[MessageLevel],
		   HostAdapter->HostNumber, Buffer);
	  else printk("%s%s", BusLogic_MessageLevelMap[MessageLevel], Buffer);
	}
      else printk("%s", Buffer);
    }
  BeginningOfLine = (Buffer[Length-1] == '\n');
}


/*
  BusLogic_ParseKeyword parses an individual option keyword.  It returns true
  and updates the pointer if the keyword is recognized and false otherwise.
*/

static boolean BusLogic_ParseKeyword(char **StringPointer, char *Keyword)
{
  char *Pointer = *StringPointer;
  while (*Keyword != '\0')
    {
      char StringChar = *Pointer++;
      char KeywordChar = *Keyword++;
      if (StringChar >= 'A' && StringChar <= 'Z')
	StringChar += 'a' - 'Z';
      if (KeywordChar >= 'A' && KeywordChar <= 'Z')
	KeywordChar += 'a' - 'Z';
      if (StringChar != KeywordChar) return false;
    }
  *StringPointer = Pointer;
  return true;
}


/*
  BusLogic_ParseDriverOptions handles processing of BusLogic Driver Options
  specifications.

  BusLogic Driver Options may be specified either via the Linux Kernel Command
  Line or via the Loadable Kernel Module Installation Facility.  Driver Options
  for multiple host adapters may be specified either by separating the option
  strings by a semicolon, or by specifying multiple "BusLogic=" strings on the
  command line.  Individual option specifications for a single host adapter are
  separated by commas.  The Probing and Debugging Options apply to all host
  adapters whereas the remaining options apply individually only to the
  selected host adapter.

  The BusLogic Driver Probing Options comprise the following:

  IO:<integer>

    The "IO:" option specifies an ISA I/O Address to be probed for a non-PCI
    MultiMaster Host Adapter.  If neither "IO:" nor "NoProbeISA" options are
    specified, then the standard list of BusLogic MultiMaster ISA I/O Addresses
    will be probed (0x330, 0x334, 0x230, 0x234, 0x130, and 0x134).  Multiple
    "IO:" options may be specified to precisely determine the I/O Addresses to
    be probed, but the probe order will always follow the standard list.

  NoProbe

    The "NoProbe" option disables all probing and therefore no BusLogic Host
    Adapters will be detected.

  NoProbeISA

    The "NoProbeISA" option disables probing of the standard BusLogic ISA I/O
    Addresses and therefore only PCI MultiMaster and FlashPoint Host Adapters
    will be detected.

  NoProbePCI

    The "NoProbePCI" options disables the interrogation of PCI Configuration
    Space and therefore only ISA Multimaster Host Adapters will be detected, as
    well as PCI Multimaster Host Adapters that have their ISA Compatible I/O
    Port set to "Primary" or "Alternate".

  NoSortPCI

    The "NoSortPCI" option forces PCI MultiMaster Host Adapters to be
    enumerated in the order provided by the PCI BIOS, ignoring any setting of
    the AutoSCSI "Use Bus And Device # For PCI Scanning Seq." option.

  MultiMasterFirst

    The "MultiMasterFirst" option forces MultiMaster Host Adapters to be probed
    before FlashPoint Host Adapters.  By default, if both FlashPoint and PCI
    MultiMaster Host Adapters are present, this driver will probe for
    FlashPoint Host Adapters first unless the BIOS primary disk is controlled
    by the first PCI MultiMaster Host Adapter, in which case MultiMaster Host
    Adapters will be probed first.

  FlashPointFirst

    The "FlashPointFirst" option forces FlashPoint Host Adapters to be probed
    before MultiMaster Host Adapters.

  The BusLogic Driver Tagged Queuing Options allow for explicitly specifying
  the Queue Depth and whether Tagged Queuing is permitted for each Target
  Device (assuming that the Target Device supports Tagged Queuing).  The Queue
  Depth is the number of SCSI Commands that are allowed to be concurrently
  presented for execution (either to the Host Adapter or Target Device).  Note
  that explicitly enabling Tagged Queuing may lead to problems; the option to
  enable or disable Tagged Queuing is provided primarily to allow disabling
  Tagged Queuing on Target Devices that do not implement it correctly.  The
  following options are available:

  QueueDepth:<integer>

    The "QueueDepth:" or QD:" option specifies the Queue Depth to use for all
    Target Devices that support Tagged Queuing, as well as the maximum Queue
    Depth for devices that do not support Tagged Queuing.  If no Queue Depth
    option is provided, the Queue Depth will be determined automatically based
    on the Host Adapter's Total Queue Depth and the number, type, speed, and
    capabilities of the detected Target Devices.  For Host Adapters that
    require ISA Bounce Buffers, the Queue Depth is automatically set by default
    to BusLogic_TaggedQueueDepthBB or BusLogic_UntaggedQueueDepthBB to avoid
    excessive preallocation of DMA Bounce Buffer memory.  Target Devices that
    do not support Tagged Queuing always have their Queue Depth set to
    BusLogic_UntaggedQueueDepth or BusLogic_UntaggedQueueDepthBB, unless a
    lower Queue Depth option is provided.  A Queue Depth of 1 automatically
    disables Tagged Queuing.

  QueueDepth:[<integer>,<integer>...]

    The "QueueDepth:[...]" or "QD:[...]" option specifies the Queue Depth
    individually for each Target Device.  If an <integer> is omitted, the
    associated Target Device will have its Queue Depth selected automatically.

  TaggedQueuing:Default

    The "TaggedQueuing:Default" or "TQ:Default" option permits Tagged Queuing
    based on the firmware version of the BusLogic Host Adapter and based on
    whether the Queue Depth allows queuing multiple commands.

  TaggedQueuing:Enable

    The "TaggedQueuing:Enable" or "TQ:Enable" option enables Tagged Queuing for
    all Target Devices on this Host Adapter, overriding any limitation that
    would otherwise be imposed based on the Host Adapter firmware version.

  TaggedQueuing:Disable

    The "TaggedQueuing:Disable" or "TQ:Disable" option disables Tagged Queuing
    for all Target Devices on this Host Adapter.

  TaggedQueuing:<Target-Spec>

    The "TaggedQueuing:<Target-Spec>" or "TQ:<Target-Spec>" option controls
    Tagged Queuing individually for each Target Device.  <Target-Spec> is a
    sequence of "Y", "N", and "X" characters.  "Y" enables Tagged Queuing, "N"
    disables Tagged Queuing, and "X" accepts the default based on the firmware
    version.  The first character refers to Target Device 0, the second to
    Target Device 1, and so on; if the sequence of "Y", "N", and "X" characters
    does not cover all the Target Devices, unspecified characters are assumed
    to be "X".

  The BusLogic Driver Error Recovery Option allows for explicitly specifying
  the Error Recovery action to be performed when BusLogic_ResetCommand is
  called due to a SCSI Command failing to complete successfully.  The following
  options are available:

  ErrorRecovery:Default

    The "ErrorRecovery:Default" or "ER:Default" option selects between the Hard
    Reset and Bus Device Reset options based on the recommendation of the SCSI
    Subsystem.

  ErrorRecovery:HardReset

    The "ErrorRecovery:HardReset" or "ER:HardReset" option will initiate a Host
    Adapter Hard Reset which also causes a SCSI Bus Reset.

  ErrorRecovery:BusDeviceReset

    The "ErrorRecovery:BusDeviceReset" or "ER:BusDeviceReset" option will send
    a Bus Device Reset message to the individual Target Device causing the
    error.  If Error Recovery is again initiated for this Target Device and no
    SCSI Command to this Target Device has completed successfully since the Bus
    Device Reset message was sent, then a Hard Reset will be attempted.

  ErrorRecovery:None

    The "ErrorRecovery:None" or "ER:None" option suppresses Error Recovery.
    This option should only be selected if a SCSI Bus Reset or Bus Device Reset
    will cause the Target Device or a critical operation to suffer a complete
    and unrecoverable failure.

  ErrorRecovery:<Target-Spec>

    The "ErrorRecovery:<Target-Spec>" or "ER:<Target-Spec>" option controls
    Error Recovery individually for each Target Device.  <Target-Spec> is a
    sequence of "D", "H", "B", and "N" characters.  "D" selects Default, "H"
    selects Hard Reset, "B" selects Bus Device Reset, and "N" selects None.
    The first character refers to Target Device 0, the second to Target Device
    1, and so on; if the sequence of "D", "H", "B", and "N" characters does not
    cover all the possible Target Devices, unspecified characters are assumed
    to be "D".

  The BusLogic Driver Miscellaneous Options comprise the following:

  BusSettleTime:<seconds>

    The "BusSettleTime:" or "BST:" option specifies the Bus Settle Time in
    seconds.  The Bus Settle Time is the amount of time to wait between a Host
    Adapter Hard Reset which initiates a SCSI Bus Reset and issuing any SCSI
    Commands.  If unspecified, it defaults to BusLogic_DefaultBusSettleTime.

  InhibitTargetInquiry

    The "InhibitTargetInquiry" option inhibits the execution of an Inquire
    Target Devices or Inquire Installed Devices command on MultiMaster Host
    Adapters.  This may be necessary with some older Target Devices that do not
    respond correctly when Logical Units above 0 are addressed.

  The BusLogic Driver Debugging Options comprise the following:

  TraceProbe

    The "TraceProbe" option enables tracing of Host Adapter Probing.

  TraceHardwareReset

    The "TraceHardwareReset" option enables tracing of Host Adapter Hardware
    Reset.

  TraceConfiguration

    The "TraceConfiguration" option enables tracing of Host Adapter
    Configuration.

  TraceErrors

    The "TraceErrors" option enables tracing of SCSI Commands that return an
    error from the Target Device.  The CDB and Sense Data will be printed for
    each SCSI Command that fails.

  Debug

    The "Debug" option enables all debugging options.

  The following examples demonstrate setting the Queue Depth for Target Devices
  1 and 2 on the first host adapter to 7 and 15, the Queue Depth for all Target
  Devices on the second host adapter to 31, and the Bus Settle Time on the
  second host adapter to 30 seconds.

  Linux Kernel Command Line:

    linux BusLogic=QueueDepth:[,7,15];QueueDepth:31,BusSettleTime:30

  LILO Linux Boot Loader (in /etc/lilo.conf):

    append = "BusLogic=QueueDepth:[,7,15];QueueDepth:31,BusSettleTime:30"

  INSMOD Loadable Kernel Module Installation Facility:

    insmod BusLogic.o \
	'BusLogic="QueueDepth:[,7,15];QueueDepth:31,BusSettleTime:30"'

  NOTE: Module Utilities 2.1.71 or later is required for correct parsing
	of driver options containing commas.

*/

static int __init BusLogic_ParseDriverOptions(char *OptionsString)
{
  while (true)
    {
      BusLogic_DriverOptions_T *DriverOptions =
	&BusLogic_DriverOptions[BusLogic_DriverOptionsCount++];
      int TargetID;
      memset(DriverOptions, 0, sizeof(BusLogic_DriverOptions_T));
      for (TargetID = 0; TargetID < BusLogic_MaxTargetDevices; TargetID++)
	DriverOptions->ErrorRecoveryStrategy[TargetID] =
	  BusLogic_ErrorRecovery_Default;
      while (*OptionsString != '\0' && *OptionsString != ';')
	{
	  /* Probing Options. */
	  if (BusLogic_ParseKeyword(&OptionsString, "IO:"))
	    {
	      BusLogic_IO_Address_T IO_Address =
		simple_strtoul(OptionsString, &OptionsString, 0);
	      BusLogic_ProbeOptions.LimitedProbeISA = true;
	      switch (IO_Address)
		{
		case 0x330:
		  BusLogic_ProbeOptions.Probe330 = true;
		  break;
		case 0x334:
		  BusLogic_ProbeOptions.Probe334 = true;
		  break;
		case 0x230:
		  BusLogic_ProbeOptions.Probe230 = true;
		  break;
		case 0x234:
		  BusLogic_ProbeOptions.Probe234 = true;
		  break;
		case 0x130:
		  BusLogic_ProbeOptions.Probe130 = true;
		  break;
		case 0x134:
		  BusLogic_ProbeOptions.Probe134 = true;
		  break;
		default:
		  BusLogic_Error("BusLogic: Invalid Driver Options "
				 "(illegal I/O Address 0x%X)\n",
				 NULL, IO_Address);
		  return 0;
		}
	    }
	  else if (BusLogic_ParseKeyword(&OptionsString, "NoProbeISA"))
	    BusLogic_ProbeOptions.NoProbeISA = true;
	  else if (BusLogic_ParseKeyword(&OptionsString, "NoProbePCI"))
	    BusLogic_ProbeOptions.NoProbePCI = true;
	  else if (BusLogic_ParseKeyword(&OptionsString, "NoProbe"))
	    BusLogic_ProbeOptions.NoProbe = true;
	  else if (BusLogic_ParseKeyword(&OptionsString, "NoSortPCI"))
	    BusLogic_ProbeOptions.NoSortPCI = true;
	  else if (BusLogic_ParseKeyword(&OptionsString, "MultiMasterFirst"))
	    BusLogic_ProbeOptions.MultiMasterFirst = true;
	  else if (BusLogic_ParseKeyword(&OptionsString, "FlashPointFirst"))
	    BusLogic_ProbeOptions.FlashPointFirst = true;
	  /* Tagged Queuing Options. */
	  else if (BusLogic_ParseKeyword(&OptionsString, "QueueDepth:[") ||
		   BusLogic_ParseKeyword(&OptionsString, "QD:["))
	    {
	      for (TargetID = 0;
		   TargetID < BusLogic_MaxTargetDevices;
		   TargetID++)
		{
		  unsigned short QueueDepth =
		    simple_strtoul(OptionsString, &OptionsString, 0);
		  if (QueueDepth > BusLogic_MaxTaggedQueueDepth)
		    {
		      BusLogic_Error("BusLogic: Invalid Driver Options "
				     "(illegal Queue Depth %d)\n",
				     NULL, QueueDepth);
		      return 0;
		    }
		  DriverOptions->QueueDepth[TargetID] = QueueDepth;
		  if (*OptionsString == ',')
		    OptionsString++;
		  else if (*OptionsString == ']')
		    break;
		  else
		    {
		      BusLogic_Error("BusLogic: Invalid Driver Options "
				     "(',' or ']' expected at '%s')\n",
				     NULL, OptionsString);
		      return 0;
		    }
		}
	      if (*OptionsString != ']')
		{
		  BusLogic_Error("BusLogic: Invalid Driver Options "
				 "(']' expected at '%s')\n",
				 NULL, OptionsString);
		  return 0;
		}
	      else OptionsString++;
	    }
	  else if (BusLogic_ParseKeyword(&OptionsString, "QueueDepth:") ||
		   BusLogic_ParseKeyword(&OptionsString, "QD:"))
	    {
	      unsigned short QueueDepth =
		simple_strtoul(OptionsString, &OptionsString, 0);
	      if (QueueDepth == 0 || QueueDepth > BusLogic_MaxTaggedQueueDepth)
		{
		  BusLogic_Error("BusLogic: Invalid Driver Options "
				 "(illegal Queue Depth %d)\n",
				 NULL, QueueDepth);
		  return 0;
		}
	      DriverOptions->CommonQueueDepth = QueueDepth;
	      for (TargetID = 0;
		   TargetID < BusLogic_MaxTargetDevices;
		   TargetID++)
		DriverOptions->QueueDepth[TargetID] = QueueDepth;
	    }
	  else if (BusLogic_ParseKeyword(&OptionsString, "TaggedQueuing:") ||
		   BusLogic_ParseKeyword(&OptionsString, "TQ:"))
	    {
	      if (BusLogic_ParseKeyword(&OptionsString, "Default"))
		{
		  DriverOptions->TaggedQueuingPermitted = 0x0000;
		  DriverOptions->TaggedQueuingPermittedMask = 0x0000;
		}
	      else if (BusLogic_ParseKeyword(&OptionsString, "Enable"))
		{
		  DriverOptions->TaggedQueuingPermitted = 0xFFFF;
		  DriverOptions->TaggedQueuingPermittedMask = 0xFFFF;
		}
	      else if (BusLogic_ParseKeyword(&OptionsString, "Disable"))
		{
		  DriverOptions->TaggedQueuingPermitted = 0x0000;
		  DriverOptions->TaggedQueuingPermittedMask = 0xFFFF;
		}
	      else
		{
		  unsigned short TargetBit;
		  for (TargetID = 0, TargetBit = 1;
		       TargetID < BusLogic_MaxTargetDevices;
		       TargetID++, TargetBit <<= 1)
		    switch (*OptionsString++)
		      {
		      case 'Y':
			DriverOptions->TaggedQueuingPermitted |= TargetBit;
			DriverOptions->TaggedQueuingPermittedMask |= TargetBit;
			break;
		      case 'N':
			DriverOptions->TaggedQueuingPermitted &= ~TargetBit;
			DriverOptions->TaggedQueuingPermittedMask |= TargetBit;
			break;
		      case 'X':
			break;
		      default:
			OptionsString--;
			TargetID = BusLogic_MaxTargetDevices;
			break;
		      }
		}
	    }
	  /* Error Recovery Option. */
	  else if (BusLogic_ParseKeyword(&OptionsString, "ErrorRecovery:") ||
		   BusLogic_ParseKeyword(&OptionsString, "ER:"))
	    {
	      if (BusLogic_ParseKeyword(&OptionsString, "Default"))
		for (TargetID = 0;
		     TargetID < BusLogic_MaxTargetDevices;
		     TargetID++)
		  DriverOptions->ErrorRecoveryStrategy[TargetID] =
		    BusLogic_ErrorRecovery_Default;
	      else if (BusLogic_ParseKeyword(&OptionsString, "HardReset"))
		for (TargetID = 0;
		     TargetID < BusLogic_MaxTargetDevices;
		     TargetID++)
		  DriverOptions->ErrorRecoveryStrategy[TargetID] =
		    BusLogic_ErrorRecovery_HardReset;
	      else if (BusLogic_ParseKeyword(&OptionsString, "BusDeviceReset"))
		for (TargetID = 0;
		     TargetID < BusLogic_MaxTargetDevices;
		     TargetID++)
		  DriverOptions->ErrorRecoveryStrategy[TargetID] =
		    BusLogic_ErrorRecovery_BusDeviceReset;
	      else if (BusLogic_ParseKeyword(&OptionsString, "None"))
		for (TargetID = 0;
		     TargetID < BusLogic_MaxTargetDevices;
		     TargetID++)
		  DriverOptions->ErrorRecoveryStrategy[TargetID] =
		    BusLogic_ErrorRecovery_None;
	      else
		for (TargetID = 0;
		     TargetID < BusLogic_MaxTargetDevices;
		     TargetID++)
		  switch (*OptionsString++)
		    {
		    case 'D':
		      DriverOptions->ErrorRecoveryStrategy[TargetID] =
			BusLogic_ErrorRecovery_Default;
		      break;
		    case 'H':
		      DriverOptions->ErrorRecoveryStrategy[TargetID] =
			BusLogic_ErrorRecovery_HardReset;
		      break;
		    case 'B':
		      DriverOptions->ErrorRecoveryStrategy[TargetID] =
			BusLogic_ErrorRecovery_BusDeviceReset;
		      break;
		    case 'N':
		      DriverOptions->ErrorRecoveryStrategy[TargetID] =
			BusLogic_ErrorRecovery_None;
		      break;
		    default:
		      OptionsString--;
		      TargetID = BusLogic_MaxTargetDevices;
		      break;
		    }
	    }
	  /* Miscellaneous Options. */
	  else if (BusLogic_ParseKeyword(&OptionsString, "BusSettleTime:") ||
		   BusLogic_ParseKeyword(&OptionsString, "BST:"))
	    {
	      unsigned short BusSettleTime =
		simple_strtoul(OptionsString, &OptionsString, 0);
	      if (BusSettleTime > 5 * 60)
		{
		  BusLogic_Error("BusLogic: Invalid Driver Options "
				 "(illegal Bus Settle Time %d)\n",
				 NULL, BusSettleTime);
		  return 0;
		}
	      DriverOptions->BusSettleTime = BusSettleTime;
	    }
	  else if (BusLogic_ParseKeyword(&OptionsString,
					 "InhibitTargetInquiry"))
	    DriverOptions->LocalOptions.InhibitTargetInquiry = true;
	  /* Debugging Options. */
	  else if (BusLogic_ParseKeyword(&OptionsString, "TraceProbe"))
	      BusLogic_GlobalOptions.TraceProbe = true;
	  else if (BusLogic_ParseKeyword(&OptionsString, "TraceHardwareReset"))
	      BusLogic_GlobalOptions.TraceHardwareReset = true;
	  else if (BusLogic_ParseKeyword(&OptionsString, "TraceConfiguration"))
	      BusLogic_GlobalOptions.TraceConfiguration = true;
	  else if (BusLogic_ParseKeyword(&OptionsString, "TraceErrors"))
	      BusLogic_GlobalOptions.TraceErrors = true;
	  else if (BusLogic_ParseKeyword(&OptionsString, "Debug"))
	    {
	      BusLogic_GlobalOptions.TraceProbe = true;
	      BusLogic_GlobalOptions.TraceHardwareReset = true;
	      BusLogic_GlobalOptions.TraceConfiguration = true;
	      BusLogic_GlobalOptions.TraceErrors = true;
	    }
	  if (*OptionsString == ',')
	    OptionsString++;
	  else if (*OptionsString != ';' && *OptionsString != '\0')
	    {
	      BusLogic_Error("BusLogic: Unexpected Driver Option '%s' "
			     "ignored\n", NULL, OptionsString);
	      *OptionsString = '\0';
	    }
	}
      if (!(BusLogic_DriverOptionsCount == 0 ||
	    BusLogic_ProbeInfoCount == 0 ||
	    BusLogic_DriverOptionsCount == BusLogic_ProbeInfoCount))
	{
	  BusLogic_Error("BusLogic: Invalid Driver Options "
			 "(all or no I/O Addresses must be specified)\n", NULL);
	  return 0;
	}
      /*
	Tagged Queuing is disabled when the Queue Depth is 1 since queuing
	multiple commands is not possible.
      */
      for (TargetID = 0; TargetID < BusLogic_MaxTargetDevices; TargetID++)
	if (DriverOptions->QueueDepth[TargetID] == 1)
	  {
	    unsigned short TargetBit = 1 << TargetID;
	    DriverOptions->TaggedQueuingPermitted &= ~TargetBit;
	    DriverOptions->TaggedQueuingPermittedMask |= TargetBit;
	  }
      if (*OptionsString == ';') OptionsString++;
      if (*OptionsString == '\0') return 0;
    }
    return 1;
}


/*
  BusLogic_Setup handles processing of Kernel Command Line Arguments.
*/

static int __init 
BusLogic_Setup(char *str)
{
	int ints[3];

	(void)get_options(str, ARRAY_SIZE(ints), ints);

	if (ints[0] != 0) {
		BusLogic_Error("BusLogic: Obsolete Command Line Entry "
				"Format Ignored\n", NULL);
		return 0;
	}
	if (str == NULL || *str == '\0')
		return 0;
	return BusLogic_ParseDriverOptions(str);
}

__setup("BusLogic=", BusLogic_Setup);

/*
  Get it all started
*/
MODULE_LICENSE("GPL");

static SCSI_Host_Template_T driver_template = BUSLOGIC;

#include "scsi_module.c"
