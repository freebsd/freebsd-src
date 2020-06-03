/** @file
  UEFI Multicast Trivial File Transfer Protocol v6 Definition, which is built upon
  the EFI UDPv6 Protocol and provides basic services for client-side unicast and/or
  multicast TFTP operations.

  Copyright (c) 2008 - 2011, Intel Corporation. All rights reserved.<BR>
  (C) Copyright 2016 Hewlett Packard Enterprise Development LP<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This Protocol is introduced in UEFI Specification 2.2

**/

#ifndef __EFI_MTFTP6_PROTOCOL_H__
#define __EFI_MTFTP6_PROTOCOL_H__


#define EFI_MTFTP6_SERVICE_BINDING_PROTOCOL_GUID \
  { \
    0xd9760ff3, 0x3cca, 0x4267, {0x80, 0xf9, 0x75, 0x27, 0xfa, 0xfa, 0x42, 0x23 } \
  }

#define EFI_MTFTP6_PROTOCOL_GUID \
  { \
    0xbf0a78ba, 0xec29, 0x49cf, {0xa1, 0xc9, 0x7a, 0xe5, 0x4e, 0xab, 0x6a, 0x51 } \
  }

typedef struct _EFI_MTFTP6_PROTOCOL EFI_MTFTP6_PROTOCOL;
typedef struct _EFI_MTFTP6_TOKEN    EFI_MTFTP6_TOKEN;

///
/// MTFTP Packet OpCodes
///@{
#define EFI_MTFTP6_OPCODE_RRQ      1 ///< The MTFTPv6 packet is a read request.
#define EFI_MTFTP6_OPCODE_WRQ      2 ///< The MTFTPv6 packet is a write request.
#define EFI_MTFTP6_OPCODE_DATA     3 ///< The MTFTPv6 packet is a data packet.
#define EFI_MTFTP6_OPCODE_ACK      4 ///< The MTFTPv6 packet is an acknowledgement packet.
#define EFI_MTFTP6_OPCODE_ERROR    5 ///< The MTFTPv6 packet is an error packet.
#define EFI_MTFTP6_OPCODE_OACK     6 ///< The MTFTPv6 packet is an option acknowledgement packet.
#define EFI_MTFTP6_OPCODE_DIR      7 ///< The MTFTPv6 packet is a directory query packet.
#define EFI_MTFTP6_OPCODE_DATA8    8 ///< The MTFTPv6 packet is a data packet with a big block number.
#define EFI_MTFTP6_OPCODE_ACK8     9 ///< The MTFTPv6 packet is an acknowledgement packet with a big block number.
///@}

///
/// MTFTP ERROR Packet ErrorCodes
///@{
///
/// The error code is not defined. See the error message in the packet (if any) for details.
///
#define EFI_MTFTP6_ERRORCODE_NOT_DEFINED           0
///
/// The file was not found.
///
#define EFI_MTFTP6_ERRORCODE_FILE_NOT_FOUND        1
///
/// There was an access violation.
///
#define EFI_MTFTP6_ERRORCODE_ACCESS_VIOLATION      2
///
/// The disk was full or its allocation was exceeded.
///
#define EFI_MTFTP6_ERRORCODE_DISK_FULL             3
///
/// The MTFTPv6 operation was illegal.
///
#define EFI_MTFTP6_ERRORCODE_ILLEGAL_OPERATION     4
///
/// The transfer ID is unknown.
///
#define EFI_MTFTP6_ERRORCODE_UNKNOWN_TRANSFER_ID   5
///
/// The file already exists.
///
#define EFI_MTFTP6_ERRORCODE_FILE_ALREADY_EXISTS   6
///
/// There is no such user.
///
#define EFI_MTFTP6_ERRORCODE_NO_SUCH_USER          7
///
/// The request has been denied due to option negotiation.
///
#define EFI_MTFTP6_ERRORCODE_REQUEST_DENIED        8
///@}

#pragma pack(1)

///
/// EFI_MTFTP6_REQ_HEADER
///
typedef struct {
  ///
  /// For this packet type, OpCode = EFI_MTFTP6_OPCODE_RRQ for a read request
  /// or OpCode = EFI_MTFTP6_OPCODE_WRQ for a write request.
  ///
  UINT16    OpCode;
  ///
  /// The file name to be downloaded or uploaded.
  ///
  UINT8     Filename[1];
} EFI_MTFTP6_REQ_HEADER;

///
/// EFI_MTFTP6_OACK_HEADER
///
typedef struct {
  ///
  /// For this packet type, OpCode = EFI_MTFTP6_OPCODE_OACK.
  ///
  UINT16    OpCode;
  ///
  /// The option strings in the option acknowledgement packet.
  ///
  UINT8     Data[1];
} EFI_MTFTP6_OACK_HEADER;

///
/// EFI_MTFTP6_DATA_HEADER
///
typedef struct {
  ///
  /// For this packet type, OpCode = EFI_MTFTP6_OPCODE_DATA.
  ///
  UINT16    OpCode;
  ///
  /// Block number of this data packet.
  ///
  UINT16    Block;
  ///
  /// The content of this data packet.
  ///
  UINT8     Data[1];
} EFI_MTFTP6_DATA_HEADER;

///
/// EFI_MTFTP6_ACK_HEADER
///
typedef struct {
  ///
  /// For this packet type, OpCode = EFI_MTFTP6_OPCODE_ACK.
  ///
  UINT16    OpCode;
  ///
  /// The block number of the data packet that is being acknowledged.
  ///
  UINT16    Block[1];
} EFI_MTFTP6_ACK_HEADER;

///
/// EFI_MTFTP6_DATA8_HEADER
///
typedef struct {
  ///
  /// For this packet type, OpCode = EFI_MTFTP6_OPCODE_DATA8.
  ///
  UINT16    OpCode;
  ///
  /// The block number of data packet.
  ///
  UINT64    Block;
  ///
  /// The content of this data packet.
  ///
  UINT8     Data[1];
} EFI_MTFTP6_DATA8_HEADER;

///
/// EFI_MTFTP6_ACK8_HEADER
///
typedef struct {
  ///
  /// For this packet type, OpCode = EFI_MTFTP6_OPCODE_ACK8.
  ///
  UINT16    OpCode;
  ///
  /// The block number of the data packet that is being acknowledged.
  ///
  UINT64    Block[1];
} EFI_MTFTP6_ACK8_HEADER;

///
/// EFI_MTFTP6_ERROR_HEADER
///
typedef struct {
  ///
  /// For this packet type, OpCode = EFI_MTFTP6_OPCODE_ERROR.
  ///
  UINT16    OpCode;
  ///
  /// The error number as defined by the MTFTPv6 packet error codes.
  ///
  UINT16    ErrorCode;
  ///
  /// Error message string.
  ///
  UINT8     ErrorMessage[1];
} EFI_MTFTP6_ERROR_HEADER;

///
/// EFI_MTFTP6_PACKET
///
typedef union {
  UINT16                   OpCode; ///< Type of packets as defined by the MTFTPv6 packet opcodes.
  EFI_MTFTP6_REQ_HEADER    Rrq;    ///< Read request packet header.
  EFI_MTFTP6_REQ_HEADER    Wrq;    ///< write request packet header.
  EFI_MTFTP6_OACK_HEADER   Oack;   ///< Option acknowledge packet header.
  EFI_MTFTP6_DATA_HEADER   Data;   ///< Data packet header.
  EFI_MTFTP6_ACK_HEADER    Ack;    ///< Acknowledgement packet header.
  EFI_MTFTP6_DATA8_HEADER  Data8;  ///< Data packet header with big block number.
  EFI_MTFTP6_ACK8_HEADER   Ack8;   ///< Acknowledgement header with big block number.
  EFI_MTFTP6_ERROR_HEADER  Error;  ///< Error packet header.
} EFI_MTFTP6_PACKET;

#pragma pack()

///
/// EFI_MTFTP6_CONFIG_DATA
///
typedef struct {
  ///
  /// The local IP address to use. Set to zero to let the underlying IPv6
  /// driver choose a source address. If not zero it must be one of the
  /// configured IP addresses in the underlying IPv6 driver.
  ///
  EFI_IPv6_ADDRESS       StationIp;
  ///
  /// Local port number. Set to zero to use the automatically assigned port number.
  ///
  UINT16                 LocalPort;
  ///
  /// The IP address of the MTFTPv6 server.
  ///
  EFI_IPv6_ADDRESS       ServerIp;
  ///
  /// The initial MTFTPv6 server port number. Request packets are
  /// sent to this port. This number is almost always 69 and using zero
  /// defaults to 69.
  UINT16                 InitialServerPort;
  ///
  /// The number of times to transmit MTFTPv6 request packets and wait for a response.
  ///
  UINT16                 TryCount;
  ///
  /// The number of seconds to wait for a response after sending the MTFTPv6 request packet.
  ///
  UINT16                 TimeoutValue;
} EFI_MTFTP6_CONFIG_DATA;

///
/// EFI_MTFTP6_MODE_DATA
///
typedef struct {
  ///
  /// The configuration data of this instance.
  ///
  EFI_MTFTP6_CONFIG_DATA  ConfigData;
  ///
  /// The number of option strings in the following SupportedOptions array.
  ///
  UINT8                   SupportedOptionCount;
  ///
  /// An array of null-terminated ASCII option strings that are recognized and supported by
  /// this EFI MTFTPv6 Protocol driver implementation. The buffer is
  /// read only to the caller and the caller should NOT free the buffer.
  ///
  UINT8                   **SupportedOptions;
} EFI_MTFTP6_MODE_DATA;

///
/// EFI_MTFTP_OVERRIDE_DATA
///
typedef struct {
  ///
  /// IP address of the MTFTPv6 server. If set to all zero, the value that
  /// was set by the EFI_MTFTP6_PROTOCOL.Configure() function will be used.
  ///
  EFI_IPv6_ADDRESS       ServerIp;
  ///
  /// MTFTPv6 server port number. If set to zero, it will use the value
  /// that was set by the EFI_MTFTP6_PROTOCOL.Configure() function.
  ///
  UINT16                 ServerPort;
  ///
  /// Number of times to transmit MTFTPv6 request packets and wait
  /// for a response. If set to zero, the value that was set by
  /// theEFI_MTFTP6_PROTOCOL.Configure() function will be used.
  ///
  UINT16                 TryCount;
  ///
  /// Number of seconds to wait for a response after sending the
  /// MTFTPv6 request packet. If set to zero, the value that was set by
  /// the EFI_MTFTP6_PROTOCOL.Configure() function will be used.
  ///
  UINT16                 TimeoutValue;
} EFI_MTFTP6_OVERRIDE_DATA;

///
/// EFI_MTFTP6_OPTION
///
typedef struct {
  UINT8                  *OptionStr; ///< Pointer to the null-terminated ASCII MTFTPv6 option string.
  UINT8                  *ValueStr;  ///< Pointer to the null-terminated ASCII MTFTPv6 value string.
} EFI_MTFTP6_OPTION;

/**
  EFI_MTFTP6_TIMEOUT_CALLBACK is a callback function that the caller provides to capture the
  timeout event in the EFI_MTFTP6_PROTOCOL.ReadFile(), EFI_MTFTP6_PROTOCOL.WriteFile() or
  EFI_MTFTP6_PROTOCOL.ReadDirectory() functions.

  Whenever a timeout occurs, the EFI MTFTPv6 Protocol driver will call the EFI_MTFTP6_TIMEOUT_CALLBACK
  function to notify the caller of the timeout event. Any status code other than EFI_SUCCESS
  that is returned from this function will abort the current download process.

  @param[in] This          Pointer to the EFI_MTFTP6_PROTOCOL instance.
  @param[in] Token         The token that the caller provided in the EFI_MTFTP6_PROTOCOl.ReadFile(),
                           WriteFile() or ReadDirectory() function.
  @param[in] PacketLen     Indicates the length of the packet.
  @param[in] Packet        Pointer to an MTFTPv6 packet.

  @retval EFI_SUCCESS      Operation success.
  @retval Others           Aborts session.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_MTFTP6_CHECK_PACKET)(
  IN EFI_MTFTP6_PROTOCOL      *This,
  IN EFI_MTFTP6_TOKEN         *Token,
  IN UINT16                   PacketLen,
  IN EFI_MTFTP6_PACKET        *Packet
  );

/**
  EFI_MTFTP6_TIMEOUT_CALLBACK is a callback function that the caller provides to capture the
  timeout event in the EFI_MTFTP6_PROTOCOL.ReadFile(), EFI_MTFTP6_PROTOCOL.WriteFile() or
  EFI_MTFTP6_PROTOCOL.ReadDirectory() functions.

  Whenever a timeout occurs, the EFI MTFTPv6 Protocol driver will call the EFI_MTFTP6_TIMEOUT_CALLBACK
  function to notify the caller of the timeout event. Any status code other than EFI_SUCCESS
  that is returned from this function will abort the current download process.

  @param[in]      This     Pointer to the EFI_MTFTP6_PROTOCOL instance.
  @param[in]      Token    The token that is provided in the EFI_MTFTP6_PROTOCOL.ReadFile() or
                           EFI_MTFTP6_PROTOCOL.WriteFile() or EFI_MTFTP6_PROTOCOL.ReadDirectory()
                           functions by the caller.

  @retval EFI_SUCCESS      Operation success.
  @retval Others           Aborts session.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_MTFTP6_TIMEOUT_CALLBACK)(
  IN EFI_MTFTP6_PROTOCOL      *This,
  IN EFI_MTFTP6_TOKEN         *Token
  );

/**
  EFI_MTFTP6_PACKET_NEEDED is a callback function that the caller provides to feed data to the
  EFI_MTFTP6_PROTOCOL.WriteFile() function.

  EFI_MTFTP6_PACKET_NEEDED provides another mechanism for the caller to provide data to upload
  other than a static buffer. The EFI MTFTP6 Protocol driver always calls EFI_MTFTP6_PACKET_NEEDED
  to get packet data from the caller if no static buffer was given in the initial call to
  EFI_MTFTP6_PROTOCOL.WriteFile() function. Setting *Length to zero signals the end of the session.
  Returning a status code other than EFI_SUCCESS aborts the session.

  @param[in]      This     Pointer to the EFI_MTFTP6_PROTOCOL instance.
  @param[in]      Token    The token provided in the EFI_MTFTP6_PROTOCOL.WriteFile() by the caller.
  @param[in, out] Length   Indicates the length of the raw data wanted on input, and the
                           length the data available on output.
  @param[out]     Buffer   Pointer to the buffer where the data is stored.

  @retval EFI_SUCCESS      Operation success.
  @retval Others           Aborts session.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_MTFTP6_PACKET_NEEDED)(
  IN EFI_MTFTP6_PROTOCOL      *This,
  IN EFI_MTFTP6_TOKEN         *Token,
  IN OUT UINT16               *Length,
  OUT VOID                    **Buffer
  );

struct _EFI_MTFTP6_TOKEN {
  ///
  /// The status that is returned to the caller at the end of the operation
  /// to indicate whether this operation completed successfully.
  /// Defined Status values are listed below.
  ///
  EFI_STATUS                  Status;
  ///
  /// The event that will be signaled when the operation completes. If
  /// set to NULL, the corresponding function will wait until the read or
  /// write operation finishes. The type of Event must be EVT_NOTIFY_SIGNAL.
  ///
  EFI_EVENT                   Event;
  ///
  /// If not NULL, the data that will be used to override the existing
  /// configure data.
  ///
  EFI_MTFTP6_OVERRIDE_DATA    *OverrideData;
  ///
  /// Pointer to the null-terminated ASCII file name string.
  ///
  UINT8                       *Filename;
  ///
  /// Pointer to the null-terminated ASCII mode string. If NULL, octet is used.
  ///
  UINT8                       *ModeStr;
  ///
  /// Number of option/value string pairs.
  ///
  UINT32                      OptionCount;
  ///
  /// Pointer to an array of option/value string pairs. Ignored if
  /// OptionCount is zero. Both a remote server and this driver
  /// implementation should support these options. If one or more
  /// options are unrecognized by this implementation, it is sent to the
  /// remote server without being changed.
  ///
  EFI_MTFTP6_OPTION           *OptionList;
  ///
  /// On input, the size, in bytes, of Buffer. On output, the number
  /// of bytes transferred.
  ///
  UINT64                      BufferSize;
  ///
  /// Pointer to the data buffer. Data that is downloaded from the
  /// MTFTPv6 server is stored here. Data that is uploaded to the
  /// MTFTPv6 server is read from here. Ignored if BufferSize is zero.
  ///
  VOID                        *Buffer;
  ///
  /// Pointer to the context that will be used by CheckPacket,
  /// TimeoutCallback and PacketNeeded.
  ///
  VOID                        *Context;
  ///
  /// Pointer to the callback function to check the contents of the
  /// received packet.
  ///
  EFI_MTFTP6_CHECK_PACKET      CheckPacket;
  ///
  /// Pointer to the function to be called when a timeout occurs.
  ///
  EFI_MTFTP6_TIMEOUT_CALLBACK  TimeoutCallback;
  ///
  /// Pointer to the function to provide the needed packet contents.
  /// Only used in WriteFile() operation.
  ///
  EFI_MTFTP6_PACKET_NEEDED     PacketNeeded;
};

/**
  Read the current operational settings.

  The GetModeData() function reads the current operational settings of this EFI MTFTPv6
  Protocol driver instance.

  @param[in]  This               Pointer to the EFI_MTFTP6_PROTOCOL instance.
  @param[out] ModeData           The buffer in which the EFI MTFTPv6 Protocol driver mode
                                 data is returned.

  @retval  EFI_SUCCESS           The configuration data was successfully returned.
  @retval  EFI_OUT_OF_RESOURCES  The required mode data could not be allocated.
  @retval  EFI_INVALID_PARAMETER This is NULL or ModeData is NULL.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_MTFTP6_GET_MODE_DATA)(
  IN EFI_MTFTP6_PROTOCOL      *This,
  OUT EFI_MTFTP6_MODE_DATA    *ModeData
  );

/**
  Initializes, changes, or resets the default operational setting for this EFI MTFTPv6
  Protocol driver instance.

  The Configure() function is used to set and change the configuration data for this EFI
  MTFTPv6 Protocol driver instance. The configuration data can be reset to startup defaults by calling
  Configure() with MtftpConfigData set to NULL. Whenever the instance is reset, any
  pending operation is aborted. By changing the EFI MTFTPv6 Protocol driver instance configuration
  data, the client can connect to different MTFTPv6 servers. The configuration parameters in
  MtftpConfigData are used as the default parameters in later MTFTPv6 operations and can be
  overridden in later operations.

  @param[in]  This               Pointer to the EFI_MTFTP6_PROTOCOL instance.
  @param[in]  MtftpConfigData    Pointer to the configuration data structure.

  @retval  EFI_SUCCESS           The EFI MTFTPv6 Protocol instance was configured successfully.
  @retval  EFI_INVALID_PARAMETER One or more following conditions are TRUE:
                                 - This is NULL.
                                 - MtftpConfigData.StationIp is neither zero nor one
                                   of the configured IP addresses in the underlying IPv6 driver.
                                 - MtftpCofigData.ServerIp is not a valid IPv6 unicast address.
  @retval  EFI_ACCESS_DENIED     - The configuration could not be changed at this time because there
                                   is some MTFTP background operation in progress.
                                 - MtftpCofigData.LocalPort is already in use.
  @retval  EFI_NO_MAPPING        The underlying IPv6 driver was responsible for choosing a source
                                 address for this instance, but no source address was available for use.
  @retval  EFI_OUT_OF_RESOURCES  The EFI MTFTPv6 Protocol driver instance data could not be
                                 allocated.
  @retval  EFI_DEVICE_ERROR      An unexpected system or network error occurred. The EFI
                                 MTFTPv6 Protocol driver instance is not configured.


**/
typedef
EFI_STATUS
(EFIAPI *EFI_MTFTP6_CONFIGURE)(
  IN EFI_MTFTP6_PROTOCOL      *This,
  IN EFI_MTFTP6_CONFIG_DATA   *MtftpConfigData OPTIONAL
);

/**
  Get information about a file from an MTFTPv6 server.

  The GetInfo() function assembles an MTFTPv6 request packet with options, sends it to the
  MTFTPv6 server, and may return an MTFTPv6 OACK, MTFTPv6 ERROR, or ICMP ERROR packet.
  Retries occur only if no response packets are received from the MTFTPv6 server before the
  timeout expires.

  @param[in]  This               Pointer to the EFI_MTFTP6_PROTOCOL instance.
  @param[in]  OverrideData       Data that is used to override the existing parameters. If NULL, the
                                 default parameters that were set in the EFI_MTFTP6_PROTOCOL.Configure()
                                 function are used.
  @param[in]  Filename           Pointer to null-terminated ASCII file name string.
  @param[in]  ModeStr            Pointer to null-terminated ASCII mode string. If NULL, octet will be used
  @param[in]  OptionCount        Number of option/value string pairs in OptionList.
  @param[in]  OptionList         Pointer to array of option/value string pairs. Ignored if
                                 OptionCount is zero.
  @param[out] PacketLength       The number of bytes in the returned packet.
  @param[out] Packet             The pointer to the received packet. This buffer must be freed by
                                 the caller.

  @retval  EFI_SUCCESS              An MTFTPv6 OACK packet was received and is in the Packet.
  @retval  EFI_INVALID_PARAMETER    One or more of the following conditions is TRUE:
                                    - This is NULL.
                                    - Filename is NULL
                                    - OptionCount is not zero and OptionList is NULL.
                                    - One or more options in OptionList have wrong format.
                                    - PacketLength is NULL.
                                    - OverrideData.ServerIp is not valid unicast IPv6 addresses.
  @retval  EFI_UNSUPPORTED          One or more options in the OptionList are unsupported by
                                    this implementation.
  @retval  EFI_NOT_STARTED          The EFI MTFTPv6 Protocol driver has not been started.
  @retval  EFI_NO_MAPPING           The underlying IPv6 driver was responsible for choosing a source
                                    address for this instance, but no source address was available for use.
  @retval  EFI_ACCESS_DENIED        The previous operation has not completed yet.
  @retval  EFI_OUT_OF_RESOURCES     Required system resources could not be allocated.
  @retval  EFI_TFTP_ERROR           An MTFTPv6 ERROR packet was received and is in the Packet.
  @retval  EFI_NETWORK_UNREACHABLE  An ICMP network unreachable error packet was received and the Packet is set to NULL.
  @retval  EFI_HOST_UNREACHABLE     An ICMP host unreachable error packet was received and the Packet is set to NULL.
  @retval  EFI_PROTOCOL_UNREACHABLE An ICMP protocol unreachable error packet was received and the Packet is set to NULL.
  @retval  EFI_PORT_UNREACHABLE     An ICMP port unreachable error packet was received and the Packet is set to NULL.
  @retval  EFI_ICMP_ERROR           Some other ICMP ERROR packet was received and the Packet is set to NULL.
  @retval  EFI_PROTOCOL_ERROR       An unexpected MTFTPv6 packet was received and is in the Packet.
  @retval  EFI_TIMEOUT              No responses were received from the MTFTPv6 server.
  @retval  EFI_DEVICE_ERROR         An unexpected network error or system error occurred.
  @retval  EFI_NO_MEDIA             There was a media error.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_MTFTP6_GET_INFO)(
  IN EFI_MTFTP6_PROTOCOL      *This,
  IN EFI_MTFTP6_OVERRIDE_DATA *OverrideData OPTIONAL,
  IN UINT8                    *Filename,
  IN UINT8                    *ModeStr OPTIONAL,
  IN UINT8                    OptionCount,
  IN EFI_MTFTP6_OPTION        *OptionList OPTIONAL,
  OUT UINT32                  *PacketLength,
  OUT EFI_MTFTP6_PACKET       **Packet OPTIONAL
);

/**
  Parse the options in an MTFTPv6 OACK packet.

  The ParseOptions() function parses the option fields in an MTFTPv6 OACK packet and
  returns the number of options that were found and optionally a list of pointers to
  the options in the packet.
  If one or more of the option fields are not valid, then EFI_PROTOCOL_ERROR is returned
  and *OptionCount and *OptionList stop at the last valid option.

  @param[in]  This               Pointer to the EFI_MTFTP6_PROTOCOL instance.
  @param[in]  PacketLen          Length of the OACK packet to be parsed.
  @param[in]  Packet             Pointer to the OACK packet to be parsed.
  @param[out] OptionCount        Pointer to the number of options in the following OptionList.
  @param[out] OptionList         Pointer to EFI_MTFTP6_OPTION storage. Each pointer in the
                                 OptionList points to the corresponding MTFTP option buffer
                                 in the Packet. Call the EFI Boot Service FreePool() to
                                 release the OptionList if the options in this OptionList
                                 are not needed any more.

  @retval  EFI_SUCCESS           The OACK packet was valid and the OptionCount and
                                 OptionList parameters have been updated.
  @retval  EFI_INVALID_PARAMETER One or more of the following conditions is TRUE:
                                 - PacketLen is 0.
                                 - Packet is NULL or Packet is not a valid MTFTPv6 packet.
                                 - OptionCount is NULL.
  @retval  EFI_NOT_FOUND         No options were found in the OACK packet.
  @retval  EFI_OUT_OF_RESOURCES  Storage for the OptionList array can not be allocated.
  @retval  EFI_PROTOCOL_ERROR    One or more of the option fields is invalid.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_MTFTP6_PARSE_OPTIONS)(
  IN EFI_MTFTP6_PROTOCOL      *This,
  IN UINT32                   PacketLen,
  IN EFI_MTFTP6_PACKET        *Packet,
  OUT UINT32                  *OptionCount,
  OUT EFI_MTFTP6_OPTION       **OptionList OPTIONAL
  );

/**
  Download a file from an MTFTPv6 server.

  The ReadFile() function is used to initialize and start an MTFTPv6 download process and
  optionally wait for completion. When the download operation completes, whether successfully or
  not, the Token.Status field is updated by the EFI MTFTPv6 Protocol driver and then
  Token.Event is signaled if it is not NULL.

  Data can be downloaded from the MTFTPv6 server into either of the following locations:
  - A fixed buffer that is pointed to by Token.Buffer
  - A download service function that is pointed to by Token.CheckPacket

  If both Token.Buffer and Token.CheckPacket are used, then Token.CheckPacket
  will be called first. If the call is successful, the packet will be stored in Token.Buffer.

  @param[in]  This               Pointer to the EFI_MTFTP6_PROTOCOL instance.
  @param[in]  Token              Pointer to the token structure to provide the parameters that are
                                 used in this operation.

  @retval  EFI_SUCCESS              The data file has been transferred successfully.
  @retval  EFI_OUT_OF_RESOURCES     Required system resources could not be allocated.
  @retval  EFI_BUFFER_TOO_SMALL     BufferSize is not zero but not large enough to hold the
                                    downloaded data in downloading process.
  @retval  EFI_ABORTED              Current operation is aborted by user.
  @retval  EFI_NETWORK_UNREACHABLE  An ICMP network unreachable error packet was received.
  @retval  EFI_HOST_UNREACHABLE     An ICMP host unreachable error packet was received.
  @retval  EFI_PROTOCOL_UNREACHABLE An ICMP protocol unreachable error packet was received.
  @retval  EFI_PORT_UNREACHABLE     An ICMP port unreachable error packet was received.
  @retval  EFI_ICMP_ERROR           An ICMP ERROR packet was received.
  @retval  EFI_TIMEOUT              No responses were received from the MTFTPv6 server.
  @retval  EFI_TFTP_ERROR           An MTFTPv6 ERROR packet was received.
  @retval  EFI_DEVICE_ERROR         An unexpected network error or system error occurred.
  @retval  EFI_NO_MEDIA             There was a media error.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_MTFTP6_READ_FILE)(
  IN EFI_MTFTP6_PROTOCOL      *This,
  IN EFI_MTFTP6_TOKEN         *Token
  );

/**
  Send a file to an MTFTPv6 server. May be unsupported in some implementations.

  The WriteFile() function is used to initialize an uploading operation with the given option list
  and optionally wait for completion. If one or more of the options is not supported by the server, the
  unsupported options are ignored and a standard TFTP process starts instead. When the upload
  process completes, whether successfully or not, Token.Event is signaled, and the EFI MTFTPv6
  Protocol driver updates Token.Status.

  The caller can supply the data to be uploaded in the following two modes:
  - Through the user-provided buffer
  - Through a callback function

  With the user-provided buffer, the Token.BufferSize field indicates the length of the buffer,
  and the driver will upload the data in the buffer. With an EFI_MTFTP6_PACKET_NEEDED
  callback function, the driver will call this callback function to get more data from the user to upload.
  See the definition of EFI_MTFTP6_PACKET_NEEDED for more information. These two modes
  cannot be used at the same time. The callback function will be ignored if the user provides the
  buffer.

  @param[in]  This               Pointer to the EFI_MTFTP6_PROTOCOL instance.
  @param[in]  Token              Pointer to the token structure to provide the parameters that are
                                 used in this operation.

  @retval  EFI_SUCCESS           The upload session has started.
  @retval  EFI_UNSUPPORTED       The operation is not supported by this implementation.
  @retval  EFI_INVALID_PARAMETER One or more of the following conditions is TRUE:
                                 - This is NULL.
                                 - Token is NULL.
                                 - Token.Filename is NULL.
                                 - Token.OptionCount is not zero and Token.OptionList is NULL.
                                 - One or more options in Token.OptionList have wrong format.
                                 - Token.Buffer and Token.PacketNeeded are both NULL.
                                 - Token.OverrideData.ServerIp is not valid unicast IPv6 addresses.
  @retval  EFI_UNSUPPORTED       One or more options in the Token.OptionList are not
                                 supported by this implementation.
  @retval  EFI_NOT_STARTED       The EFI MTFTPv6 Protocol driver has not been started.
  @retval  EFI_NO_MAPPING        The underlying IPv6 driver was responsible for choosing a source
                                 address for this instance, but no source address was available for use.
  @retval  EFI_ALREADY_STARTED   This Token is already being used in another MTFTPv6 session.
  @retval  EFI_OUT_OF_RESOURCES  Required system resources could not be allocated.
  @retval  EFI_ACCESS_DENIED     The previous operation has not completed yet.
  @retval  EFI_DEVICE_ERROR      An unexpected network error or system error occurred.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_MTFTP6_WRITE_FILE)(
  IN EFI_MTFTP6_PROTOCOL      *This,
  IN EFI_MTFTP6_TOKEN         *Token
  );

/**
  Download a data file directory from an MTFTPv6 server. May be unsupported in some implementations.

  The ReadDirectory() function is used to return a list of files on the MTFTPv6 server that are
  logically (or operationally) related to Token.Filename. The directory request packet that is sent
  to the server is built with the option list that was provided by caller, if present.

  The file information that the server returns is put into either of the following locations:
  - A fixed buffer that is pointed to by Token.Buffer
  - A download service function that is pointed to by Token.CheckPacket

  If both Token.Buffer and Token.CheckPacket are used, then Token.CheckPacket
  will be called first. If the call is successful, the packet will be stored in Token.Buffer.

  The returned directory listing in the Token.Buffer or EFI_MTFTP6_PACKET consists of a list
  of two or three variable-length ASCII strings, each terminated by a null character, for each file in the
  directory. If the multicast option is involved, the first field of each directory entry is the static
  multicast IP address and UDP port number that is associated with the file name. The format of the
  field is ip:ip:ip:ip:port. If the multicast option is not involved, this field and its terminating
  null character are not present.

  The next field of each directory entry is the file name and the last field is the file information string.
  The information string contains the file size and the create/modify timestamp. The format of the
  information string is filesize yyyy-mm-dd hh:mm:ss:ffff. The timestamp is
  Coordinated Universal Time (UTC; also known as Greenwich Mean Time [GMT]).

  @param[in]  This               Pointer to the EFI_MTFTP6_PROTOCOL instance.
  @param[in]  Token              Pointer to the token structure to provide the parameters that are
                                 used in this operation.

  @retval  EFI_SUCCESS           The MTFTPv6 related file "directory" has been downloaded.
  @retval  EFI_UNSUPPORTED       The EFI MTFTPv6 Protocol driver does not support this function.
  @retval  EFI_INVALID_PARAMETER One or more of the following conditions is TRUE:
                                 - This is NULL.
                                 - Token is NULL.
                                 - Token.Filename is NULL.
                                 - Token.OptionCount is not zero and Token.OptionList is NULL.
                                 - One or more options in Token.OptionList have wrong format.
                                 - Token.Buffer and Token.CheckPacket are both NULL.
                                 - Token.OverrideData.ServerIp is not valid unicast IPv6 addresses.
  @retval  EFI_UNSUPPORTED       One or more options in the Token.OptionList are not
                                 supported by this implementation.
  @retval  EFI_NOT_STARTED       The EFI MTFTPv6 Protocol driver has not been started.
  @retval  EFI_NO_MAPPING        The underlying IPv6 driver was responsible for choosing a source
                                 address for this instance, but no source address was available for use.
  @retval  EFI_ALREADY_STARTED   This Token is already being used in another MTFTPv6 session.
  @retval  EFI_OUT_OF_RESOURCES  Required system resources could not be allocated.
  @retval  EFI_ACCESS_DENIED     The previous operation has not completed yet.
  @retval  EFI_DEVICE_ERROR      An unexpected network error or system error occurred.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_MTFTP6_READ_DIRECTORY)(
  IN EFI_MTFTP6_PROTOCOL      *This,
  IN EFI_MTFTP6_TOKEN         *Token
);

/**
  Polls for incoming data packets and processes outgoing data packets.

  The Poll() function can be used by network drivers and applications to increase the rate that data
  packets are moved between the communications device and the transmit and receive queues.
  In some systems, the periodic timer event in the managed network driver may not poll the
  underlying communications device fast enough to transmit and/or receive all data packets without
  missing incoming packets or dropping outgoing packets. Drivers and applications that are
  experiencing packet loss should try calling the Poll() function more often.

  @param[in]  This               Pointer to the EFI_MTFTP6_PROTOCOL instance.

  @retval  EFI_SUCCESS           Incoming or outgoing data was processed.
  @retval  EFI_NOT_STARTED       This EFI MTFTPv6 Protocol instance has not been started.
  @retval  EFI_INVALID_PARAMETER This is NULL.
  @retval  EFI_DEVICE_ERROR      An unexpected system or network error occurred.
  @retval  EFI_TIMEOUT           Data was dropped out of the transmit and/or receive queue.
                                 Consider increasing the polling rate.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_MTFTP6_POLL)(
  IN EFI_MTFTP6_PROTOCOL      *This
  );

///
/// The EFI_MTFTP6_PROTOCOL is designed to be used by UEFI drivers and applications to transmit
/// and receive data files. The EFI MTFTPv6 Protocol driver uses the underlying EFI UDPv6 Protocol
/// driver and EFI IPv6 Protocol driver.
///
struct _EFI_MTFTP6_PROTOCOL {
  EFI_MTFTP6_GET_MODE_DATA  GetModeData;
  EFI_MTFTP6_CONFIGURE      Configure;
  EFI_MTFTP6_GET_INFO       GetInfo;
  EFI_MTFTP6_PARSE_OPTIONS  ParseOptions;
  EFI_MTFTP6_READ_FILE      ReadFile;
  EFI_MTFTP6_WRITE_FILE     WriteFile;
  EFI_MTFTP6_READ_DIRECTORY ReadDirectory;
  EFI_MTFTP6_POLL           Poll;
};

extern EFI_GUID gEfiMtftp6ServiceBindingProtocolGuid;
extern EFI_GUID gEfiMtftp6ProtocolGuid;

#endif

