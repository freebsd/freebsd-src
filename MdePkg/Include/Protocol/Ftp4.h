/** @file
  EFI FTPv4 (File Transfer Protocol version 4) Protocol Definition
  The EFI FTPv4 Protocol is used to locate communication devices that are
  supported by an EFI FTPv4 Protocol driver and to create and destroy instances
  of the EFI FTPv4 Protocol child protocol driver that can use the underlying
  communication device.
  The definitions in this file are defined in UEFI Specification 2.3, which have
  not been verified by one implementation yet.

  Copyright (c) 2009 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This Protocol is introduced in UEFI Specification 2.2

**/

#ifndef __EFI_FTP4_PROTOCOL_H__
#define __EFI_FTP4_PROTOCOL_H__


#define EFI_FTP4_SERVICE_BINDING_PROTOCOL_GUID \
  { \
    0xfaaecb1, 0x226e, 0x4782, {0xaa, 0xce, 0x7d, 0xb9, 0xbc, 0xbf, 0x4d, 0xaf } \
  }

#define EFI_FTP4_PROTOCOL_GUID \
  { \
    0xeb338826, 0x681b, 0x4295, {0xb3, 0x56, 0x2b, 0x36, 0x4c, 0x75, 0x7b, 0x9 } \
  }

typedef struct _EFI_FTP4_PROTOCOL EFI_FTP4_PROTOCOL;

///
/// EFI_FTP4_CONNECTION_TOKEN
///
typedef struct {
  ///
  /// The Event to signal after the connection is established and Status field is updated
  /// by the EFI FTP v4 Protocol driver. The type of Event must be
  /// EVENT_NOTIFY_SIGNAL, and its Task Priority Level (TPL) must be lower than or
  /// equal to TPL_CALLBACK. If it is set to NULL, this function will not return  until the
  /// function completes.
  ///
  EFI_EVENT                            Event;
  ///
  /// The variable to receive the result of the completed operation.
  /// EFI_SUCCESS:              The FTP connection is established successfully
  /// EFI_ACCESS_DENIED:        The FTP server denied the access the user's request to access it.
  /// EFI_CONNECTION_RESET:     The connect fails because the connection is reset either by instance
  ///                           itself or communication peer.
  /// EFI_TIMEOUT:              The connection establishment timer expired and no more specific
  ///                           information is available.
  /// EFI_NETWORK_UNREACHABLE:  The active open fails because an ICMP network unreachable error is
  ///                           received.
  /// EFI_HOST_UNREACHABLE:     The active open fails because an ICMP host unreachable error is
  ///                           received.
  /// EFI_PROTOCOL_UNREACHABLE: The active open fails because an ICMP protocol unreachable error is
  ///                           received.
  /// EFI_PORT_UNREACHABLE:     The connection establishment timer times out and an ICMP port
  ///                           unreachable error is received.
  /// EFI_ICMP_ERROR:           The connection establishment timer timeout and some other ICMP
  ///                           error is received.
  /// EFI_DEVICE_ERROR:         An unexpected system or network error occurred.
  ///
  EFI_STATUS                           Status;
} EFI_FTP4_CONNECTION_TOKEN;

///
/// EFI_FTP4_CONFIG_DATA
///
typedef struct {
  ///
  /// Pointer to a ASCII string that contains user name. The caller is
  /// responsible for freeing Username after GetModeData() is called.
  ///
  UINT8                                *Username;
  ///
  /// Pointer to a ASCII string that contains password. The caller is
  /// responsible for freeing Password after GetModeData() is called.
  ///
  UINT8                                *Password;
  ///
  /// Set it to TRUE to initiate an active data connection. Set it to
  /// FALSE to initiate a passive data connection.
  ///
  BOOLEAN                              Active;
  ///
  /// Boolean value indicating if default network settting used.
  ///
  BOOLEAN                              UseDefaultSetting;
  ///
  /// IP address of station if UseDefaultSetting is FALSE.
  ///
  EFI_IPv4_ADDRESS                     StationIp;
  ///
  /// Subnet mask of station if UseDefaultSetting is FALSE.
  ///
  EFI_IPv4_ADDRESS                     SubnetMask;
  ///
  /// IP address of gateway if UseDefaultSetting is FALSE.
  ///
  EFI_IPv4_ADDRESS                     GatewayIp;
  ///
  /// IP address of FTPv4 server.
  ///
  EFI_IPv4_ADDRESS                     ServerIp;
  ///
  /// FTPv4 server port number of control connection, and the default
  /// value is 21 as convention.
  ///
  UINT16                               ServerPort;
  ///
  /// FTPv4 server port number of data connection. If it is zero, use
  /// (ServerPort - 1) by convention.
  ///
  UINT16                               AltDataPort;
  ///
  /// A byte indicate the representation type. The right 4 bit is used for
  /// first parameter, the left 4 bit is use for second parameter
  /// - For the first parameter, 0x0 = image, 0x1 = EBCDIC, 0x2 = ASCII, 0x3 = local
  /// - For the second parameter, 0x0 = Non-print, 0x1 = Telnet format effectors, 0x2 =
  ///   Carriage Control.
  /// - If it is a local type, the second parameter is the local byte byte size.
  /// - If it is a image type, the second parameter is undefined.
  ///
  UINT8                                RepType;
  ///
  /// Defines the file structure in FTP used. 0x00 = file, 0x01 = record, 0x02 = page.
  ///
  UINT8                                FileStruct;
  ///
  /// Defines the transifer mode used in FTP. 0x00 = stream, 0x01 = Block, 0x02 = Compressed.
  ///
  UINT8                                TransMode;
} EFI_FTP4_CONFIG_DATA;

typedef struct _EFI_FTP4_COMMAND_TOKEN EFI_FTP4_COMMAND_TOKEN;

/**
  Callback function when process inbound or outbound data.

  If it is receiving function that leads to inbound data, the callback function
  is called when data buffer is full. Then, old data in the data buffer should be
  flushed and new data is stored from the beginning of data buffer.
  If it is a transmit function that lead to outbound data and the size of
  Data in daata buffer has been transmitted, this callback function is called to
  supply additional data to be transmitted.

  @param[in] This                Pointer to the EFI_FTP4_PROTOCOL instance.
  @param[in] Token               Pointer to the token structure to provide the parameters that
                                 are used in this operation.
  @return  User defined Status.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_FTP4_DATA_CALLBACK)(
  IN EFI_FTP4_PROTOCOL           *This,
  IN EFI_FTP4_COMMAND_TOKEN      *Token
  );

///
/// EFI_FTP4_COMMAND_TOKEN
///
struct _EFI_FTP4_COMMAND_TOKEN {
  ///
  /// The Event to signal after request is finished and Status field
  /// is updated by the EFI FTP v4 Protocol driver. The type of Event
  /// must be EVT_NOTIFY_SIGNAL, and its Task Priority Level
  /// (TPL) must be lower than or equal to TPL_CALLBACK. If it is
  /// set to NULL, related function must wait until the function
  /// completes.
  ///
  EFI_EVENT                             Event;
  ///
  /// Pointer to a null-terminated ASCII name string.
  ///
  UINT8                                 *Pathname;
  ///
  /// The size of data buffer in bytes.
  ///
  UINT64                                DataBufferSize;
  ///
  /// Pointer to the data buffer. Data downloaded from FTP server
  /// through connection is downloaded here.
  ///
  VOID                                  *DataBuffer;
  ///
  /// Pointer to a callback function. If it is receiving function that leads
  /// to inbound data, the callback function is called when databuffer is
  /// full. Then, old data in the data buffer should be flushed and new
  /// data is stored from the beginning of data buffer. If it is a transmit
  /// function that lead to outbound data and DataBufferSize of
  /// Data in DataBuffer has been transmitted, this callback
  /// function is called to supply additional data to be transmitted. The
  /// size of additional data to be transmitted is indicated in
  /// DataBufferSize, again. If there is no data remained,
  /// DataBufferSize should be set to 0.
  ///
  EFI_FTP4_DATA_CALLBACK                DataCallback;
  ///
  /// Pointer to the parameter for DataCallback.
  ///
  VOID                                  *Context;
  ///
  /// The variable to receive the result of the completed operation.
  /// EFI_SUCCESS:              The FTP command is completed successfully.
  /// EFI_ACCESS_DENIED:        The FTP server denied the access to the requested file.
  /// EFI_CONNECTION_RESET:     The connect fails because the connection is reset either
  ///                           by instance itself or communication peer.
  /// EFI_TIMEOUT:              The connection establishment timer expired and no more
  ///                           specific information is available.
  /// EFI_NETWORK_UNREACHABLE:  The active open fails because an ICMP network unreachable
  ///                           error is received.
  /// EFI_HOST_UNREACHABLE:     The active open fails because an ICMP host unreachable
  ///                           error is received.
  /// EFI_PROTOCOL_UNREACHABLE: The active open fails because an ICMP protocol unreachable
  ///                           error is received.
  /// EFI_PORT_UNREACHABLE:     The connection establishment timer times out and an ICMP port
  ///                           unreachable error is received.
  /// EFI_ICMP_ERROR:           The connection establishment timer timeout and some other ICMP
  ///                           error is received.
  /// EFI_DEVICE_ERROR:         An unexpected system or network error occurred.
  ///
  EFI_STATUS                            Status;
};

/**
  Gets the current operational settings.

  The GetModeData() function reads the current operational settings of this
  EFI FTPv4 Protocol driver instance. EFI_FTP4_CONFIG_DATA  is defined in the
  EFI_FTP4_PROTOCOL.Configure.

  @param[in]  This               Pointer to the EFI_FTP4_PROTOCOL instance.
  @param[out] ModeData           Pointer to storage for the EFI FTPv4 Protocol driver
                                 mode data. The string buffers for Username and Password
                                 in EFI_FTP4_CONFIG_DATA are allocated by the function,
                                 and the caller should take the responsibility to free the
                                 buffer later.

  @retval EFI_SUCCESS            This function is called successfully.
  @retval EFI_INVALID_PARAMETER  One or more of the following are TRUE:
                                 - This is NULL.
                                 - ModeData is NULL.
  @retval EFI_NOT_STARTED        The EFI FTPv4 Protocol driver has not been started
  @retval EFI_OUT_OF_RESOURCES   Could not allocate enough resource to finish the operation.
  @retval EFI_DEVICE_ERROR       An unexpected system or network error occurred.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_FTP4_GET_MODE_DATA)(
  IN EFI_FTP4_PROTOCOL        *This,
  OUT EFI_FTP4_CONFIG_DATA    *ModeData
  );

/**
  Disconnecting a FTP connection gracefully.

  The Connect() function will initiate a connection request to the remote FTP server
  with the corresponding connection token. If this function returns EFI_SUCCESS, the
  connection sequence is initiated successfully.  If the connection succeeds or faild
  due to any error, the Token->Event will be signaled and Token->Status will be updated
  accordingly.

  @param[in] This                Pointer to the EFI_FTP4_PROTOCOL instance.
  @param[in] Token               Pointer to the token used to establish control connection.

  @retval EFI_SUCCESS            The connection sequence is successfully initiated.
  @retval EFI_INVALID_PARAMETER  One or more of the following are TRUE:
                                 - This is NULL.
                                 - Token is NULL.
                                 - Token->Event is NULL.
  @retval EFI_NOT_STARTED        The EFI FTPv4 Protocol driver has not been started.
  @retval EFI_NO_MAPPING         When using a default address, configuration (DHCP, BOOTP,
                                 RARP, etc.) is not finished yet.
  @retval EFI_OUT_OF_RESOURCES   Could not allocate enough resource to finish the operation.
  @retval EFI_DEVICE_ERROR       An unexpected system or network error occurred.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_FTP4_CONNECT)(
  IN EFI_FTP4_PROTOCOL           *This,
  IN EFI_FTP4_CONNECTION_TOKEN   *Token
  );

/**
  Disconnecting a FTP connection gracefully.

  The Close() function will initiate a close request to the remote FTP server with the
  corresponding connection token. If this function returns EFI_SUCCESS, the control
  connection with the remote FTP server is closed.

  @param[in] This                Pointer to the EFI_FTP4_PROTOCOL instance.
  @param[in] Token               Pointer to the token used to close control connection.

  @retval EFI_SUCCESS            The close request is successfully initiated.
  @retval EFI_INVALID_PARAMETER  One or more of the following are TRUE:
                                 - This is NULL.
                                 - Token is NULL.
                                 - Token->Event is NULL.
  @retval EFI_NOT_STARTED        The EFI FTPv4 Protocol driver has not been started.
  @retval EFI_NO_MAPPING         When using a default address, configuration (DHCP, BOOTP,
                                 RARP, etc.) is not finished yet.
  @retval EFI_OUT_OF_RESOURCES   Could not allocate enough resource to finish the operation.
  @retval EFI_DEVICE_ERROR       An unexpected system or network error occurred.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_FTP4_CLOSE)(
  IN EFI_FTP4_PROTOCOL             *This,
  IN EFI_FTP4_CONNECTION_TOKEN     *Token
  );

/**
  Sets or clears the operational parameters for the FTP child driver.

  The Configure() function will configure the connected FTP session with the
  configuration setting specified in  FtpConfigData. The configuration data can
  be reset by calling Configure() with FtpConfigData set to NULL.

  @param[in] This                Pointer to the EFI_FTP4_PROTOCOL instance.
  @param[in] FtpConfigData       Pointer to configuration data that will be assigned to
                                 the FTP child driver instance. If NULL, the FTP child
                                 driver instance is reset to startup defaults and all
                                 pending transmit and receive requests are flushed.

  @retval EFI_SUCCESS            The FTPv4 driver was configured successfully.
  @retval EFI_INVALID_PARAMETER  One or more following conditions are TRUE:
                                 - This is NULL.
                                 - FtpConfigData.RepType is invalid.
                                 - FtpConfigData.FileStruct is invalid.
                                 - FtpConfigData.TransMode is invalid.
                                 - IP address in FtpConfigData is invalid.
  @retval EFI_NO_MAPPING         When using a default address, configuration (DHCP, BOOTP,
                                 RARP, etc.) is not finished yet.
  @retval EFI_UNSUPPORTED        One or more of the configuration parameters are not supported
                                 by this implementation.
  @retval EFI_OUT_OF_RESOURCES   The EFI FTPv4 Protocol driver instance data could not be
                                 allocated.
  @retval EFI_DEVICE_ERROR       An unexpected system or network error occurred. The EFI FTPv4
                                 Protocol driver instance is not configured.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_FTP4_CONFIGURE)(
  IN EFI_FTP4_PROTOCOL           *This,
  IN EFI_FTP4_CONFIG_DATA        *FtpConfigData OPTIONAL
  );


/**
  Downloads a file from an FTPv4 server.

  The ReadFile() function is used to initialize and start an FTPv4 download process
  and optionally wait for completion. When the download operation completes, whether
  successfully or not, the Token.Status field is updated by the EFI FTPv4 Protocol
  driver and then Token.Event is signaled (if it is not NULL).

  Data will be downloaded from the FTPv4 server into Token.DataBuffer. If the file size
  is larger than Token.DataBufferSize, Token.DataCallback will be called to allow for
  processing data and then new data will be placed at the beginning of Token.DataBuffer.

  @param[in] This                Pointer to the EFI_FTP4_PROTOCOL instance.
  @param[in] Token               Pointer to the token structure to provide the parameters that
                                 are used in this operation.

  @retval EFI_SUCCESS            The data file is being downloaded successfully.
  @retval EFI_INVALID_PARAMETER  One or more of the parameters is not valid.
                                 - This is NULL.
                                 - Token is NULL.
                                 - Token.Pathname is NULL.
                                 - Token. DataBuffer is NULL.
                                 - Token. DataBufferSize is 0.
  @retval EFI_NOT_STARTED        The EFI FTPv4 Protocol driver has not been started.
  @retval EFI_NO_MAPPING         When using a default address, configuration (DHCP, BOOTP,
                                 RARP, etc.) is not finished yet.
  @retval EFI_OUT_OF_RESOURCES   Required system resources could not be allocated.
  @retval EFI_DEVICE_ERROR       An unexpected network error or system error occurred.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_FTP4_READ_FILE)(
  IN EFI_FTP4_PROTOCOL         *This,
  IN EFI_FTP4_COMMAND_TOKEN    *Token
  );

/**
  Uploads a file from an FTPv4 server.

  The WriteFile() function is used to initialize and start an FTPv4 upload process and
  optionally wait for completion. When the upload operation completes, whether successfully
  or not, the Token.Status field is updated by the EFI FTPv4 Protocol driver and then
  Token.Event is signaled (if it is not NULL). Data to be  uploaded to server is stored
  into Token.DataBuffer. Token.DataBufferSize is the number bytes to be transferred.
  If the file size is larger than Token.DataBufferSize, Token.DataCallback will be called
  to allow for processing data and then new data will be placed at the beginning of
  Token.DataBuffer. Token.DataBufferSize is updated to reflect the actual number of bytes
  to be transferred. Token.DataBufferSize is set to 0 by the call back to indicate the
  completion of data transfer.

  @param[in] This                Pointer to the EFI_FTP4_PROTOCOL instance.
  @param[in] Token               Pointer to the token structure to provide the parameters that
                                 are used in this operation.

  @retval EFI_SUCCESS            TThe data file is being uploaded successfully.
  @retval EFI_UNSUPPORTED        The operation is not supported by this implementation.
  @retval EFI_INVALID_PARAMETER  One or more of the parameters is not valid.
                                 - This is NULL.
                                 - Token is NULL.
                                 - Token.Pathname is NULL.
                                 - Token. DataBuffer is NULL.
                                 - Token. DataBufferSize is 0.
  @retval EFI_NOT_STARTED        The EFI FTPv4 Protocol driver has not been started.
  @retval EFI_NO_MAPPING         When using a default address, configuration (DHCP, BOOTP,
                                 RARP, etc.) is not finished yet.
  @retval EFI_OUT_OF_RESOURCES   Required system resources could not be allocated.
  @retval EFI_DEVICE_ERROR       An unexpected network error or system error occurred.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_FTP4_WRITE_FILE)(
  IN EFI_FTP4_PROTOCOL         *This,
  IN EFI_FTP4_COMMAND_TOKEN    *Token
  );

/**
  Download a data file "directory" from a FTPv4 server. May be unsupported in some EFI
  implementations.

  The ReadDirectory() function is used to return a list of files on the FTPv4 server that
  logically (or operationally) related to Token.Pathname, and optionally wait for completion.
  When the download operation completes, whether successfully or not, the Token.Status field
  is updated by the EFI FTPv4 Protocol driver and then Token.Event is signaled (if it is not
  NULL). Data will be downloaded from the FTPv4 server into Token.DataBuffer. If the file size
  is larger than Token.DataBufferSize, Token.DataCallback will be called to allow for processing
  data and then new data will be placed at the beginning of Token.DataBuffer.

  @param[in] This                Pointer to the EFI_FTP4_PROTOCOL instance.
  @param[in] Token               Pointer to the token structure to provide the parameters that
                                 are used in this operation.

  @retval EFI_SUCCESS            The file list information is being downloaded successfully.
  @retval EFI_UNSUPPORTED        The operation is not supported by this implementation.
  @retval EFI_INVALID_PARAMETER  One or more of the parameters is not valid.
                                 - This is NULL.
                                 - Token is NULL.
                                 - Token. DataBuffer is NULL.
                                 - Token. DataBufferSize is 0.
  @retval EFI_NOT_STARTED        The EFI FTPv4 Protocol driver has not been started.
  @retval EFI_NO_MAPPING         When using a default address, configuration (DHCP, BOOTP,
                                 RARP, etc.) is not finished yet.
  @retval EFI_OUT_OF_RESOURCES   Required system resources could not be allocated.
  @retval EFI_DEVICE_ERROR       An unexpected network error or system error occurred.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_FTP4_READ_DIRECTORY)(
  IN EFI_FTP4_PROTOCOL           *This,
  IN EFI_FTP4_COMMAND_TOKEN      *Token
  );

/**
  Polls for incoming data packets and processes outgoing data packets.

  The Poll() function can be used by network drivers and applications to increase the
  rate that data packets are moved between the communications device and the transmit
  and receive queues. In some systems, the periodic timer event in the managed network
  driver may not poll the underlying communications device fast enough to transmit
  and/or receive all data packets without missing incoming packets or dropping outgoing
  packets. Drivers and applications that are experiencing packet loss should try calling
  the Poll() function more often.

  @param[in] This                Pointer to the EFI_FTP4_PROTOCOL instance.

  @retval EFI_SUCCESS            Incoming or outgoing data was processed.
  @retval EFI_NOT_STARTED        This EFI FTPv4 Protocol instance has not been started.
  @retval EFI_INVALID_PARAMETER  This is NULL.
  @retval EFI_DEVICE_ERROR       EapAuthType An unexpected system or network error occurred.
  @retval EFI_TIMEOUT            Data was dropped out of the transmit and/or receive queue.
                                 Consider increasing the polling rate.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_FTP4_POLL)(
  IN EFI_FTP4_PROTOCOL        *This
  );

///
/// EFI_FTP4_PROTOCOL
/// provides basic services for client-side FTP (File Transfer Protocol)
/// operations.
///
struct _EFI_FTP4_PROTOCOL {
  EFI_FTP4_GET_MODE_DATA     GetModeData;
  EFI_FTP4_CONNECT           Connect;
  EFI_FTP4_CLOSE             Close;
  EFI_FTP4_CONFIGURE         Configure;
  EFI_FTP4_READ_FILE         ReadFile;
  EFI_FTP4_WRITE_FILE        WriteFile;
  EFI_FTP4_READ_DIRECTORY    ReadDirectory;
  EFI_FTP4_POLL              Poll;
};

extern EFI_GUID gEfiFtp4ServiceBindingProtocolGuid;
extern EFI_GUID gEfiFtp4ProtocolGuid;

#endif

