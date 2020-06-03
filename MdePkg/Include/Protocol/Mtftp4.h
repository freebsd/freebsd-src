/** @file
  EFI Multicast Trivial File Transfer Protocol Definition

Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This Protocol is introduced in UEFI Specification 2.0

**/

#ifndef __EFI_MTFTP4_PROTOCOL_H__
#define __EFI_MTFTP4_PROTOCOL_H__

#define EFI_MTFTP4_SERVICE_BINDING_PROTOCOL_GUID \
  { \
    0x2FE800BE, 0x8F01, 0x4aa6, {0x94, 0x6B, 0xD7, 0x13, 0x88, 0xE1, 0x83, 0x3F } \
  }

#define EFI_MTFTP4_PROTOCOL_GUID \
  { \
    0x78247c57, 0x63db, 0x4708, {0x99, 0xc2, 0xa8, 0xb4, 0xa9, 0xa6, 0x1f, 0x6b } \
  }

typedef struct _EFI_MTFTP4_PROTOCOL EFI_MTFTP4_PROTOCOL;
typedef struct _EFI_MTFTP4_TOKEN EFI_MTFTP4_TOKEN;

//
//MTFTP4 packet opcode definition
//
#define EFI_MTFTP4_OPCODE_RRQ                     1
#define EFI_MTFTP4_OPCODE_WRQ                     2
#define EFI_MTFTP4_OPCODE_DATA                    3
#define EFI_MTFTP4_OPCODE_ACK                     4
#define EFI_MTFTP4_OPCODE_ERROR                   5
#define EFI_MTFTP4_OPCODE_OACK                    6
#define EFI_MTFTP4_OPCODE_DIR                     7
#define EFI_MTFTP4_OPCODE_DATA8                   8
#define EFI_MTFTP4_OPCODE_ACK8                    9

//
// MTFTP4 error code definition
//
#define EFI_MTFTP4_ERRORCODE_NOT_DEFINED          0
#define EFI_MTFTP4_ERRORCODE_FILE_NOT_FOUND       1
#define EFI_MTFTP4_ERRORCODE_ACCESS_VIOLATION     2
#define EFI_MTFTP4_ERRORCODE_DISK_FULL            3
#define EFI_MTFTP4_ERRORCODE_ILLEGAL_OPERATION    4
#define EFI_MTFTP4_ERRORCODE_UNKNOWN_TRANSFER_ID  5
#define EFI_MTFTP4_ERRORCODE_FILE_ALREADY_EXISTS  6
#define EFI_MTFTP4_ERRORCODE_NO_SUCH_USER         7
#define EFI_MTFTP4_ERRORCODE_REQUEST_DENIED       8

//
// MTFTP4 pacekt definitions
//
#pragma pack(1)

typedef struct {
  UINT16                  OpCode;
  UINT8                   Filename[1];
} EFI_MTFTP4_REQ_HEADER;

typedef struct {
  UINT16                  OpCode;
  UINT8                   Data[1];
} EFI_MTFTP4_OACK_HEADER;

typedef struct {
  UINT16                  OpCode;
  UINT16                  Block;
  UINT8                   Data[1];
} EFI_MTFTP4_DATA_HEADER;

typedef struct {
  UINT16                  OpCode;
  UINT16                  Block[1];
} EFI_MTFTP4_ACK_HEADER;

typedef struct {
  UINT16                  OpCode;
  UINT64                  Block;
  UINT8                   Data[1];
} EFI_MTFTP4_DATA8_HEADER;

typedef struct {
  UINT16                  OpCode;
  UINT64                  Block[1];
} EFI_MTFTP4_ACK8_HEADER;

typedef struct {
  UINT16                  OpCode;
  UINT16                  ErrorCode;
  UINT8                   ErrorMessage[1];
} EFI_MTFTP4_ERROR_HEADER;

typedef union {
  ///
  /// Type of packets as defined by the MTFTPv4 packet opcodes.
  ///
  UINT16                  OpCode;
  ///
  /// Read request packet header.
  ///
  EFI_MTFTP4_REQ_HEADER   Rrq;
  ///
  /// Write request packet header.
  ///
  EFI_MTFTP4_REQ_HEADER   Wrq;
  ///
  /// Option acknowledge packet header.
  ///
  EFI_MTFTP4_OACK_HEADER  Oack;
  ///
  /// Data packet header.
  ///
  EFI_MTFTP4_DATA_HEADER  Data;
  ///
  /// Acknowledgement packet header.
  ///
  EFI_MTFTP4_ACK_HEADER   Ack;
  ///
  /// Data packet header with big block number.
  ///
  EFI_MTFTP4_DATA8_HEADER Data8;
  ///
  /// Acknowledgement header with big block num.
  ///
  EFI_MTFTP4_ACK8_HEADER  Ack8;
  ///
  /// Error packet header.
  ///
  EFI_MTFTP4_ERROR_HEADER Error;
} EFI_MTFTP4_PACKET;

#pragma pack()

///
/// MTFTP4 option definition.
///
typedef struct {
  UINT8                   *OptionStr;
  UINT8                   *ValueStr;
} EFI_MTFTP4_OPTION;


typedef struct {
  BOOLEAN                 UseDefaultSetting;
  EFI_IPv4_ADDRESS        StationIp;
  EFI_IPv4_ADDRESS        SubnetMask;
  UINT16                  LocalPort;
  EFI_IPv4_ADDRESS        GatewayIp;
  EFI_IPv4_ADDRESS        ServerIp;
  UINT16                  InitialServerPort;
  UINT16                  TryCount;
  UINT16                  TimeoutValue;
} EFI_MTFTP4_CONFIG_DATA;


typedef struct {
  EFI_MTFTP4_CONFIG_DATA  ConfigData;
  UINT8                   SupportedOptionCount;
  UINT8                   **SupportedOptoins;
  UINT8                   UnsupportedOptionCount;
  UINT8                   **UnsupportedOptoins;
} EFI_MTFTP4_MODE_DATA;


typedef struct {
  EFI_IPv4_ADDRESS        GatewayIp;
  EFI_IPv4_ADDRESS        ServerIp;
  UINT16                  ServerPort;
  UINT16                  TryCount;
  UINT16                  TimeoutValue;
} EFI_MTFTP4_OVERRIDE_DATA;

//
// Protocol interfaces definition
//

/**
  A callback function that is provided by the caller to intercept
  the EFI_MTFTP4_OPCODE_DATA or EFI_MTFTP4_OPCODE_DATA8 packets processed in the
  EFI_MTFTP4_PROTOCOL.ReadFile() function, and alternatively to intercept
  EFI_MTFTP4_OPCODE_OACK or EFI_MTFTP4_OPCODE_ERROR packets during a call to
  EFI_MTFTP4_PROTOCOL.ReadFile(), WriteFile() or ReadDirectory().

  @param  This        The pointer to the EFI_MTFTP4_PROTOCOL instance.
  @param  Token       The token that the caller provided in the
                      EFI_MTFTP4_PROTOCOL.ReadFile(), WriteFile()
                      or ReadDirectory() function.
  @param  PacketLen   Indicates the length of the packet.
  @param  Packet      The pointer to an MTFTPv4 packet.

  @retval EFI_SUCCESS The operation was successful.
  @retval Others      Aborts the transfer process.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_MTFTP4_CHECK_PACKET)(
  IN EFI_MTFTP4_PROTOCOL  *This,
  IN EFI_MTFTP4_TOKEN     *Token,
  IN UINT16               PacketLen,
  IN EFI_MTFTP4_PACKET    *Paket
  );

/**
  Timeout callback function.

  @param  This           The pointer to the EFI_MTFTP4_PROTOCOL instance.
  @param  Token          The token that is provided in the
                         EFI_MTFTP4_PROTOCOL.ReadFile() or
                         EFI_MTFTP4_PROTOCOL.WriteFile() or
                         EFI_MTFTP4_PROTOCOL.ReadDirectory() functions
                         by the caller.

  @retval EFI_SUCCESS   The operation was successful.
  @retval Others        Aborts download process.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_MTFTP4_TIMEOUT_CALLBACK)(
  IN EFI_MTFTP4_PROTOCOL  *This,
  IN EFI_MTFTP4_TOKEN     *Token
  );

/**
  A callback function that the caller provides to feed data to the
  EFI_MTFTP4_PROTOCOL.WriteFile() function.

  @param  This   The pointer to the EFI_MTFTP4_PROTOCOL instance.
  @param  Token  The token provided in the
                 EFI_MTFTP4_PROTOCOL.WriteFile() by the caller.
  @param  Length Indicates the length of the raw data wanted on input, and the
                 length the data available on output.
  @param  Buffer The pointer to the buffer where the data is stored.

  @retval EFI_SUCCESS The operation was successful.
  @retval Others      Aborts session.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_MTFTP4_PACKET_NEEDED)(
  IN  EFI_MTFTP4_PROTOCOL *This,
  IN  EFI_MTFTP4_TOKEN    *Token,
  IN  OUT UINT16          *Length,
  OUT VOID                **Buffer
  );


/**
  Submits an asynchronous interrupt transfer to an interrupt endpoint of a USB device.

  @param  This     The pointer to the EFI_MTFTP4_PROTOCOL instance.
  @param  ModeData The pointer to storage for the EFI MTFTPv4 Protocol driver mode data.

  @retval EFI_SUCCESS           The configuration data was successfully returned.
  @retval EFI_OUT_OF_RESOURCES  The required mode data could not be allocated.
  @retval EFI_INVALID_PARAMETER This is NULL or ModeData is NULL.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_MTFTP4_GET_MODE_DATA)(
  IN  EFI_MTFTP4_PROTOCOL     *This,
  OUT EFI_MTFTP4_MODE_DATA    *ModeData
  );


/**
  Initializes, changes, or resets the default operational setting for this
  EFI MTFTPv4 Protocol driver instance.

  @param  This            The pointer to the EFI_MTFTP4_PROTOCOL instance.
  @param  MtftpConfigData The pointer to the configuration data structure.

  @retval EFI_SUCCESS           The EFI MTFTPv4 Protocol driver was configured successfully.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.
  @retval EFI_ACCESS_DENIED     The EFI configuration could not be changed at this time because
                                there is one MTFTP background operation in progress.
  @retval EFI_NO_MAPPING        When using a default address, configuration (DHCP, BOOTP,
                                RARP, etc.) has not finished yet.
  @retval EFI_UNSUPPORTED       A configuration protocol (DHCP, BOOTP, RARP, etc.) could not
                                be located when clients choose to use the default address
                                settings.
  @retval EFI_OUT_OF_RESOURCES  The EFI MTFTPv4 Protocol driver instance data could not be
                                allocated.
  @retval EFI_DEVICE_ERROR      An unexpected system or network error occurred. The EFI
                                 MTFTPv4 Protocol driver instance is not configured.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_MTFTP4_CONFIGURE)(
  IN EFI_MTFTP4_PROTOCOL       *This,
  IN EFI_MTFTP4_CONFIG_DATA    *MtftpConfigData OPTIONAL
  );


/**
  Gets information about a file from an MTFTPv4 server.

  @param  This         The pointer to the EFI_MTFTP4_PROTOCOL instance.
  @param  OverrideData Data that is used to override the existing parameters. If NULL,
                       the default parameters that were set in the
                       EFI_MTFTP4_PROTOCOL.Configure() function are used.
  @param  Filename     The pointer to null-terminated ASCII file name string.
  @param  ModeStr      The pointer to null-terminated ASCII mode string. If NULL, "octet" will be used.
  @param  OptionCount  Number of option/value string pairs in OptionList.
  @param  OptionList   The pointer to array of option/value string pairs. Ignored if
                       OptionCount is zero.
  @param  PacketLength The number of bytes in the returned packet.
  @param  Packet       The pointer to the received packet. This buffer must be freed by
                       the caller.

  @retval EFI_SUCCESS              An MTFTPv4 OACK packet was received and is in the Packet.
  @retval EFI_INVALID_PARAMETER    One or more of the following conditions is TRUE:
                                   - This is NULL.
                                   - Filename is NULL.
                                   - OptionCount is not zero and OptionList is NULL.
                                   - One or more options in OptionList have wrong format.
                                   - PacketLength is NULL.
                                   - One or more IPv4 addresses in OverrideData are not valid
                                     unicast IPv4 addresses if OverrideData is not NULL.
  @retval EFI_UNSUPPORTED          One or more options in the OptionList are in the
                                   unsupported list of structure EFI_MTFTP4_MODE_DATA.
  @retval EFI_NOT_STARTED          The EFI MTFTPv4 Protocol driver has not been started.
  @retval EFI_NO_MAPPING           When using a default address, configuration (DHCP, BOOTP,
                                   RARP, etc.) has not finished yet.
  @retval EFI_ACCESS_DENIED        The previous operation has not completed yet.
  @retval EFI_OUT_OF_RESOURCES     Required system resources could not be allocated.
  @retval EFI_TFTP_ERROR           An MTFTPv4 ERROR packet was received and is in the Packet.
  @retval EFI_NETWORK_UNREACHABLE  An ICMP network unreachable error packet was received and the Packet is set to NULL.
  @retval EFI_HOST_UNREACHABLE     An ICMP host unreachable error packet was received and the Packet is set to NULL.
  @retval EFI_PROTOCOL_UNREACHABLE An ICMP protocol unreachable error packet was received and the Packet is set to NULL.
  @retval EFI_PORT_UNREACHABLE     An ICMP port unreachable error packet was received and the Packet is set to NULL.
  @retval EFI_ICMP_ERROR           Some other ICMP ERROR packet was received and is in the Buffer.
  @retval EFI_PROTOCOL_ERROR       An unexpected MTFTPv4 packet was received and is in the Packet.
  @retval EFI_TIMEOUT              No responses were received from the MTFTPv4 server.
  @retval EFI_DEVICE_ERROR         An unexpected network error or system error occurred.
  @retval EFI_NO_MEDIA             There was a media error.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_MTFTP4_GET_INFO)(
  IN  EFI_MTFTP4_PROTOCOL      *This,
  IN  EFI_MTFTP4_OVERRIDE_DATA *OverrideData   OPTIONAL,
  IN  UINT8                    *Filename,
  IN  UINT8                    *ModeStr        OPTIONAL,
  IN  UINT8                    OptionCount,
  IN  EFI_MTFTP4_OPTION        *OptionList,
  OUT UINT32                   *PacketLength,
  OUT EFI_MTFTP4_PACKET        **Packet        OPTIONAL
  );

/**
  Parses the options in an MTFTPv4 OACK packet.

  @param  This         The pointer to the EFI_MTFTP4_PROTOCOL instance.
  @param  PacketLen    Length of the OACK packet to be parsed.
  @param  Packet       The pointer to the OACK packet to be parsed.
  @param  OptionCount  The pointer to the number of options in following OptionList.
  @param  OptionList   The pointer to EFI_MTFTP4_OPTION storage. Call the EFI Boot
                       Service FreePool() to release the OptionList if the options
                       in this OptionList are not needed any more.

  @retval EFI_SUCCESS           The OACK packet was valid and the OptionCount and
                                OptionList parameters have been updated.
  @retval EFI_INVALID_PARAMETER One or more of the following conditions is TRUE:
                                - PacketLen is 0.
                                - Packet is NULL or Packet is not a valid MTFTPv4 packet.
                                - OptionCount is NULL.
  @retval EFI_NOT_FOUND         No options were found in the OACK packet.
  @retval EFI_OUT_OF_RESOURCES  Storage for the OptionList array cannot be allocated.
  @retval EFI_PROTOCOL_ERROR    One or more of the option fields is invalid.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_MTFTP4_PARSE_OPTIONS)(
  IN  EFI_MTFTP4_PROTOCOL      *This,
  IN  UINT32                   PacketLen,
  IN  EFI_MTFTP4_PACKET        *Packet,
  OUT UINT32                   *OptionCount,
  OUT EFI_MTFTP4_OPTION        **OptionList OPTIONAL
  );


/**
  Downloads a file from an MTFTPv4 server.

  @param  This  The pointer to the EFI_MTFTP4_PROTOCOL instance.
  @param  Token The pointer to the token structure to provide the parameters that are
                used in this operation.

  @retval EFI_SUCCESS              The data file has been transferred successfully.
  @retval EFI_OUT_OF_RESOURCES     Required system resources could not be allocated.
  @retval EFI_BUFFER_TOO_SMALL     BufferSize is not zero but not large enough to hold the
                                   downloaded data in downloading process.
  @retval EFI_ABORTED              Current operation is aborted by user.
  @retval EFI_NETWORK_UNREACHABLE  An ICMP network unreachable error packet was received.
  @retval EFI_HOST_UNREACHABLE     An ICMP host unreachable error packet was received.
  @retval EFI_PROTOCOL_UNREACHABLE An ICMP protocol unreachable error packet was received.
  @retval EFI_PORT_UNREACHABLE     An ICMP port unreachable error packet was received.
  @retval EFI_ICMP_ERROR           Some other  ICMP ERROR packet was received.
  @retval EFI_TIMEOUT              No responses were received from the MTFTPv4 server.
  @retval EFI_TFTP_ERROR           An MTFTPv4 ERROR packet was received.
  @retval EFI_DEVICE_ERROR         An unexpected network error or system error occurred.
  @retval EFI_NO_MEDIA             There was a media error.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_MTFTP4_READ_FILE)(
  IN EFI_MTFTP4_PROTOCOL       *This,
  IN EFI_MTFTP4_TOKEN          *Token
  );



/**
  Sends a file to an MTFTPv4 server.

  @param  This  The pointer to the EFI_MTFTP4_PROTOCOL instance.
  @param  Token The pointer to the token structure to provide the parameters that are
                used in this operation.

  @retval EFI_SUCCESS           The upload session has started.
  @retval EFI_UNSUPPORTED       The operation is not supported by this implementation.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.
  @retval EFI_UNSUPPORTED       One or more options in the Token.OptionList are in
                                the unsupported list of structure EFI_MTFTP4_MODE_DATA.
  @retval EFI_NOT_STARTED       The EFI MTFTPv4 Protocol driver has not been started.
  @retval EFI_NO_MAPPING        When using a default address, configuration (DHCP, BOOTP,
                                RARP, etc.) is not finished yet.
  @retval EFI_ALREADY_STARTED   This Token is already being used in another MTFTPv4 session.
  @retval EFI_OUT_OF_RESOURCES  Required system resources could not be allocated.
  @retval EFI_ACCESS_DENIED     The previous operation has not completed yet.
  @retval EFI_DEVICE_ERROR      An unexpected network error or system error occurred.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_MTFTP4_WRITE_FILE)(
  IN EFI_MTFTP4_PROTOCOL       *This,
  IN EFI_MTFTP4_TOKEN          *Token
  );


/**
  Downloads a data file "directory" from an MTFTPv4 server. May be unsupported in some EFI
  implementations.

  @param  This  The pointer to the EFI_MTFTP4_PROTOCOL instance.
  @param  Token The pointer to the token structure to provide the parameters that are
                used in this operation.

  @retval EFI_SUCCESS           The MTFTPv4 related file "directory" has been downloaded.
  @retval EFI_UNSUPPORTED       The operation is not supported by this implementation.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.
  @retval EFI_UNSUPPORTED       One or more options in the Token.OptionList are in
                                the unsupported list of structure EFI_MTFTP4_MODE_DATA.
  @retval EFI_NOT_STARTED       The EFI MTFTPv4 Protocol driver has not been started.
  @retval EFI_NO_MAPPING        When using a default address, configuration (DHCP, BOOTP,
                                RARP, etc.) is not finished yet.
  @retval EFI_ALREADY_STARTED   This Token is already being used in another MTFTPv4 session.
  @retval EFI_OUT_OF_RESOURCES  Required system resources could not be allocated.
  @retval EFI_ACCESS_DENIED     The previous operation has not completed yet.
  @retval EFI_DEVICE_ERROR      An unexpected network error or system error occurred.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_MTFTP4_READ_DIRECTORY)(
  IN EFI_MTFTP4_PROTOCOL       *This,
  IN EFI_MTFTP4_TOKEN          *Token
  );

/**
  Polls for incoming data packets and processes outgoing data packets.

  @param  This The pointer to the EFI_MTFTP4_PROTOCOL instance.

  @retval  EFI_SUCCESS           Incoming or outgoing data was processed.
  @retval  EFI_NOT_STARTED       This EFI MTFTPv4 Protocol instance has not been started.
  @retval  EFI_NO_MAPPING        When using a default address, configuration (DHCP, BOOTP,
                                 RARP, etc.) is not finished yet.
  @retval  EFI_INVALID_PARAMETER This is NULL.
  @retval  EFI_DEVICE_ERROR      An unexpected system or network error occurred.
  @retval  EFI_TIMEOUT           Data was dropped out of the transmit and/or receive queue.
                                 Consider increasing the polling rate.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_MTFTP4_POLL)(
  IN EFI_MTFTP4_PROTOCOL       *This
  );

///
/// The EFI_MTFTP4_PROTOCOL is designed to be used by UEFI drivers and applications
/// to transmit and receive data files. The EFI MTFTPv4 Protocol driver uses
/// the underlying EFI UDPv4 Protocol driver and EFI IPv4 Protocol driver.
///
struct _EFI_MTFTP4_PROTOCOL {
  EFI_MTFTP4_GET_MODE_DATA     GetModeData;
  EFI_MTFTP4_CONFIGURE         Configure;
  EFI_MTFTP4_GET_INFO          GetInfo;
  EFI_MTFTP4_PARSE_OPTIONS     ParseOptions;
  EFI_MTFTP4_READ_FILE         ReadFile;
  EFI_MTFTP4_WRITE_FILE        WriteFile;
  EFI_MTFTP4_READ_DIRECTORY    ReadDirectory;
  EFI_MTFTP4_POLL              Poll;
};

struct _EFI_MTFTP4_TOKEN {
  ///
  /// The status that is returned to the caller at the end of the operation
  /// to indicate whether this operation completed successfully.
  ///
  EFI_STATUS                  Status;
  ///
  /// The event that will be signaled when the operation completes. If
  /// set to NULL, the corresponding function will wait until the read or
  /// write operation finishes. The type of Event must be
  /// EVT_NOTIFY_SIGNAL. The Task Priority Level (TPL) of
  /// Event must be lower than or equal to TPL_CALLBACK.
  ///
  EFI_EVENT                   Event;
  ///
  /// If not NULL, the data that will be used to override the existing configure data.
  ///
  EFI_MTFTP4_OVERRIDE_DATA    *OverrideData;
  ///
  /// The pointer to the null-terminated ASCII file name string.
  ///
  UINT8                       *Filename;
  ///
  /// The pointer to the null-terminated ASCII mode string. If NULL, "octet" is used.
  ///
  UINT8                       *ModeStr;
  ///
  /// Number of option/value string pairs.
  ///
  UINT32                      OptionCount;
  ///
  /// The pointer to an array of option/value string pairs. Ignored if OptionCount is zero.
  ///
  EFI_MTFTP4_OPTION           *OptionList;
  ///
  /// The size of the data buffer.
  ///
  UINT64                      BufferSize;
  ///
  /// The pointer to the data buffer. Data that is downloaded from the
  /// MTFTPv4 server is stored here. Data that is uploaded to the
  /// MTFTPv4 server is read from here. Ignored if BufferSize is zero.
  ///
  VOID                        *Buffer;
  ///
  /// The pointer to the context that will be used by CheckPacket,
  /// TimeoutCallback and PacketNeeded.
  ///
  VOID                        *Context;
  ///
  /// The pointer to the callback function to check the contents of the received packet.
  ///
  EFI_MTFTP4_CHECK_PACKET     CheckPacket;
  ///
  /// The pointer to the function to be called when a timeout occurs.
  ///
  EFI_MTFTP4_TIMEOUT_CALLBACK TimeoutCallback;
  ///
  /// The pointer to the function to provide the needed packet contents.
  ///
  EFI_MTFTP4_PACKET_NEEDED    PacketNeeded;
};

extern EFI_GUID gEfiMtftp4ServiceBindingProtocolGuid;
extern EFI_GUID gEfiMtftp4ProtocolGuid;

#endif

