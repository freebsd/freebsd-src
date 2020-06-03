/** @file
  This protocol provides services that allow NVM Express commands to be sent to an
  NVM Express controller or to a specific namespace in a NVM Express controller.
  This protocol interface is optimized for storage.

  Copyright (c) 2013 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This Protocol was introduced in UEFI Specification 2.5.

**/

#ifndef _UEFI_NVM_EXPRESS_PASS_THRU_H_
#define _UEFI_NVM_EXPRESS_PASS_THRU_H_

#define EFI_NVM_EXPRESS_PASS_THRU_PROTOCOL_GUID \
  { \
    0x52c78312, 0x8edc, 0x4233, { 0x98, 0xf2, 0x1a, 0x1a, 0xa5, 0xe3, 0x88, 0xa5 } \
  }

typedef struct _EFI_NVM_EXPRESS_PASS_THRU_PROTOCOL EFI_NVM_EXPRESS_PASS_THRU_PROTOCOL;

typedef struct {
  UINT32          Attributes;
  UINT32          IoAlign;
  UINT32          NvmeVersion;
} EFI_NVM_EXPRESS_PASS_THRU_MODE;

//
// If this bit is set, then the EFI_NVM_EXPRESS_PASS_THRU_PROTOCOL interface is
// for directly addressable namespaces.
//
#define EFI_NVM_EXPRESS_PASS_THRU_ATTRIBUTES_PHYSICAL        0x0001
//
// If this bit is set, then the EFI_NVM_EXPRESS_PASS_THRU_PROTOCOL interface is
// for a single volume logical namespace comprised of multiple namespaces.
//
#define EFI_NVM_EXPRESS_PASS_THRU_ATTRIBUTES_LOGICAL         0x0002
//
// If this bit is set, then the EFI_NVM_EXPRESS_PASS_THRU_PROTOCOL interface
// supports non-blocking I/O.
//
#define EFI_NVM_EXPRESS_PASS_THRU_ATTRIBUTES_NONBLOCKIO      0x0004
//
// If this bit is set, then the EFI_NVM_EXPRESS_PASS_THRU_PROTOCOL interface
// supports NVM command set.
//
#define EFI_NVM_EXPRESS_PASS_THRU_ATTRIBUTES_CMD_SET_NVM     0x0008

//
// FusedOperation
//
#define NORMAL_CMD                  0x00
#define FUSED_FIRST_CMD             0x01
#define FUSED_SECOND_CMD            0x02

typedef struct {
  UINT32                            Opcode:8;
  UINT32                            FusedOperation:2;
  UINT32                            Reserved:22;
} NVME_CDW0;

//
// Flags
//
#define CDW2_VALID                  0x01
#define CDW3_VALID                  0x02
#define CDW10_VALID                 0x04
#define CDW11_VALID                 0x08
#define CDW12_VALID                 0x10
#define CDW13_VALID                 0x20
#define CDW14_VALID                 0x40
#define CDW15_VALID                 0x80

//
// Queue Type
//
#define NVME_ADMIN_QUEUE            0x00
#define NVME_IO_QUEUE               0x01

typedef struct {
  NVME_CDW0                         Cdw0;
  UINT8                             Flags;
  UINT32                            Nsid;
  UINT32                            Cdw2;
  UINT32                            Cdw3;
  UINT32                            Cdw10;
  UINT32                            Cdw11;
  UINT32                            Cdw12;
  UINT32                            Cdw13;
  UINT32                            Cdw14;
  UINT32                            Cdw15;
} EFI_NVM_EXPRESS_COMMAND;

typedef struct {
  UINT32                            DW0;
  UINT32                            DW1;
  UINT32                            DW2;
  UINT32                            DW3;
} EFI_NVM_EXPRESS_COMPLETION;

typedef struct {
  UINT64                            CommandTimeout;
  VOID                              *TransferBuffer;
  UINT32                            TransferLength;
  VOID                              *MetadataBuffer;
  UINT32                            MetadataLength;
  UINT8                             QueueType;
  EFI_NVM_EXPRESS_COMMAND           *NvmeCmd;
  EFI_NVM_EXPRESS_COMPLETION        *NvmeCompletion;
} EFI_NVM_EXPRESS_PASS_THRU_COMMAND_PACKET;

//
// Protocol function prototypes
//
/**
  Sends an NVM Express Command Packet to an NVM Express controller or namespace. This function supports
  both blocking I/O and non-blocking I/O. The blocking I/O functionality is required, and the non-blocking
  I/O functionality is optional.


  @param[in]     This                A pointer to the EFI_NVM_EXPRESS_PASS_THRU_PROTOCOL instance.
  @param[in]     NamespaceId         A 32 bit namespace ID as defined in the NVMe specification to which the NVM Express Command
                                     Packet will be sent.  A value of 0 denotes the NVM Express controller, a value of all 0xFF's
                                     (all bytes are 0xFF) in the namespace ID specifies that the command packet should be sent to
                                     all valid namespaces.
  @param[in,out] Packet              A pointer to the NVM Express Command Packet.
  @param[in]     Event               If non-blocking I/O is not supported then Event is ignored, and blocking I/O is performed.
                                     If Event is NULL, then blocking I/O is performed. If Event is not NULL and non-blocking I/O
                                     is supported, then non-blocking I/O is performed, and Event will be signaled when the NVM
                                     Express Command Packet completes.

  @retval EFI_SUCCESS                The NVM Express Command Packet was sent by the host. TransferLength bytes were transferred
                                     to, or from DataBuffer.
  @retval EFI_BAD_BUFFER_SIZE        The NVM Express Command Packet was not executed. The number of bytes that could be transferred
                                     is returned in TransferLength.
  @retval EFI_NOT_READY              The NVM Express Command Packet could not be sent because the controller is not ready. The caller
                                     may retry again later.
  @retval EFI_DEVICE_ERROR           A device error occurred while attempting to send the NVM Express Command Packet.
  @retval EFI_INVALID_PARAMETER      NamespaceId or the contents of EFI_NVM_EXPRESS_PASS_THRU_COMMAND_PACKET are invalid. The NVM
                                     Express Command Packet was not sent, so no additional status information is available.
  @retval EFI_UNSUPPORTED            The command described by the NVM Express Command Packet is not supported by the NVM Express
                                     controller. The NVM Express Command Packet was not sent so no additional status information
                                     is available.
  @retval EFI_TIMEOUT                A timeout occurred while waiting for the NVM Express Command Packet to execute.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_NVM_EXPRESS_PASS_THRU_PASSTHRU)(
  IN     EFI_NVM_EXPRESS_PASS_THRU_PROTOCOL          *This,
  IN     UINT32                                      NamespaceId,
  IN OUT EFI_NVM_EXPRESS_PASS_THRU_COMMAND_PACKET    *Packet,
  IN     EFI_EVENT                                   Event OPTIONAL
  );

/**
  Used to retrieve the next namespace ID for this NVM Express controller.

  The EFI_NVM_EXPRESS_PASS_THRU_PROTOCOL.GetNextNamespace() function retrieves the next valid
  namespace ID on this NVM Express controller.

  If on input the value pointed to by NamespaceId is 0xFFFFFFFF, then the first valid namespace
  ID defined on the NVM Express controller is returned in the location pointed to by NamespaceId
  and a status of EFI_SUCCESS is returned.

  If on input the value pointed to by NamespaceId is an invalid namespace ID other than 0xFFFFFFFF,
  then EFI_INVALID_PARAMETER is returned.

  If on input the value pointed to by NamespaceId is a valid namespace ID, then the next valid
  namespace ID on the NVM Express controller is returned in the location pointed to by NamespaceId,
  and EFI_SUCCESS is returned.

  If the value pointed to by NamespaceId is the namespace ID of the last namespace on the NVM
  Express controller, then EFI_NOT_FOUND is returned.

  @param[in]     This           A pointer to the EFI_NVM_EXPRESS_PASS_THRU_PROTOCOL instance.
  @param[in,out] NamespaceId    On input, a pointer to a legal NamespaceId for an NVM Express
                                namespace present on the NVM Express controller. On output, a
                                pointer to the next NamespaceId of an NVM Express namespace on
                                an NVM Express controller. An input value of 0xFFFFFFFF retrieves
                                the first NamespaceId for an NVM Express namespace present on an
                                NVM Express controller.

  @retval EFI_SUCCESS           The Namespace ID of the next Namespace was returned.
  @retval EFI_NOT_FOUND         There are no more namespaces defined on this controller.
  @retval EFI_INVALID_PARAMETER NamespaceId is an invalid value other than 0xFFFFFFFF.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_NVM_EXPRESS_PASS_THRU_GET_NEXT_NAMESPACE)(
  IN     EFI_NVM_EXPRESS_PASS_THRU_PROTOCOL          *This,
  IN OUT UINT32                                      *NamespaceId
  );

/**
  Used to allocate and build a device path node for an NVM Express namespace on an NVM Express controller.

  The EFI_NVM_EXPRESS_PASS_THRU_PROTOCOL.BuildDevicePath() function allocates and builds a single device
  path node for the NVM Express namespace specified by NamespaceId.

  If the NamespaceId is not valid, then EFI_NOT_FOUND is returned.

  If DevicePath is NULL, then EFI_INVALID_PARAMETER is returned.

  If there are not enough resources to allocate the device path node, then EFI_OUT_OF_RESOURCES is returned.

  Otherwise, DevicePath is allocated with the boot service AllocatePool(), the contents of DevicePath are
  initialized to describe the NVM Express namespace specified by NamespaceId, and EFI_SUCCESS is returned.

  @param[in]     This                A pointer to the EFI_NVM_EXPRESS_PASS_THRU_PROTOCOL instance.
  @param[in]     NamespaceId         The NVM Express namespace ID  for which a device path node is to be
                                     allocated and built. Caller must set the NamespaceId to zero if the
                                     device path node will contain a valid UUID.
  @param[out]    DevicePath          A pointer to a single device path node that describes the NVM Express
                                     namespace specified by NamespaceId. This function is responsible for
                                     allocating the buffer DevicePath with the boot service AllocatePool().
                                     It is the caller's responsibility to free DevicePath when the caller
                                     is finished with DevicePath.
  @retval EFI_SUCCESS                The device path node that describes the NVM Express namespace specified
                                     by NamespaceId was allocated and returned in DevicePath.
  @retval EFI_NOT_FOUND              The NamespaceId is not valid.
  @retval EFI_INVALID_PARAMETER      DevicePath is NULL.
  @retval EFI_OUT_OF_RESOURCES       There are not enough resources to allocate the DevicePath node.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_NVM_EXPRESS_PASS_THRU_BUILD_DEVICE_PATH)(
  IN     EFI_NVM_EXPRESS_PASS_THRU_PROTOCOL          *This,
  IN     UINT32                                      NamespaceId,
  OUT    EFI_DEVICE_PATH_PROTOCOL                    **DevicePath
  );

/**
  Used to translate a device path node to a namespace ID.

  The EFI_NVM_EXPRESS_PASS_THRU_PROTOCOL.GetNamespace() function determines the namespace ID associated with the
  namespace described by DevicePath.

  If DevicePath is a device path node type that the NVM Express Pass Thru driver supports, then the NVM Express
  Pass Thru driver will attempt to translate the contents DevicePath into a namespace ID.

  If this translation is successful, then that namespace ID is returned in NamespaceId, and EFI_SUCCESS is returned

  @param[in]  This                A pointer to the EFI_NVM_EXPRESS_PASS_THRU_PROTOCOL instance.
  @param[in]  DevicePath          A pointer to the device path node that describes an NVM Express namespace on
                                  the NVM Express controller.
  @param[out] NamespaceId         The NVM Express namespace ID contained in the device path node.

  @retval EFI_SUCCESS             DevicePath was successfully translated to NamespaceId.
  @retval EFI_INVALID_PARAMETER   If DevicePath or NamespaceId are NULL, then EFI_INVALID_PARAMETER is returned.
  @retval EFI_UNSUPPORTED         If DevicePath is not a device path node type that the NVM Express Pass Thru driver
                                  supports, then EFI_UNSUPPORTED is returned.
  @retval EFI_NOT_FOUND           If DevicePath is a device path node type that the NVM Express Pass Thru driver
                                  supports, but there is not a valid translation from DevicePath to a namespace ID,
                                  then EFI_NOT_FOUND is returned.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_NVM_EXPRESS_PASS_THRU_GET_NAMESPACE)(
  IN     EFI_NVM_EXPRESS_PASS_THRU_PROTOCOL          *This,
  IN     EFI_DEVICE_PATH_PROTOCOL                    *DevicePath,
     OUT UINT32                                      *NamespaceId
  );

//
// Protocol Interface Structure
//
struct _EFI_NVM_EXPRESS_PASS_THRU_PROTOCOL {
  EFI_NVM_EXPRESS_PASS_THRU_MODE                     *Mode;
  EFI_NVM_EXPRESS_PASS_THRU_PASSTHRU                 PassThru;
  EFI_NVM_EXPRESS_PASS_THRU_GET_NEXT_NAMESPACE       GetNextNamespace;
  EFI_NVM_EXPRESS_PASS_THRU_BUILD_DEVICE_PATH        BuildDevicePath;
  EFI_NVM_EXPRESS_PASS_THRU_GET_NAMESPACE            GetNamespace;
};

extern EFI_GUID gEfiNvmExpressPassThruProtocolGuid;

#endif

