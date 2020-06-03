/** @file
  EFI EAP Management Protocol Definition
  The EFI EAP Management Protocol is designed to provide ease of management and
  ease of test for EAPOL state machine. It is intended for the supplicant side.
  It conforms to IEEE 802.1x specification.
  The definitions in this file are defined in UEFI Specification 2.2, which have
  not been verified by one implementation yet.

  Copyright (c) 2009 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This Protocol is introduced in UEFI Specification 2.2

**/

#ifndef __EFI_EAP_MANAGEMENT_PROTOCOL_H__
#define __EFI_EAP_MANAGEMENT_PROTOCOL_H__

#include <Protocol/Eap.h>

#define EFI_EAP_MANAGEMENT_PROTOCOL_GUID \
  { \
    0xbb62e663, 0x625d, 0x40b2, {0xa0, 0x88, 0xbb, 0xe8, 0x36, 0x23, 0xa2, 0x45 } \
  }

typedef struct _EFI_EAP_MANAGEMENT_PROTOCOL EFI_EAP_MANAGEMENT_PROTOCOL;

///
/// PAE Capabilities
///
///@{
#define PAE_SUPPORT_AUTHENTICATOR       0x01
#define PAE_SUPPORT_SUPPLICANT          0x02
///@}

///
/// EFI_EAPOL_PORT_INFO
///
typedef struct _EFI_EAPOL_PORT_INFO {
  ///
  /// The identification number assigned to the Port by the System in
  /// which the Port resides.
  ///
  EFI_PORT_HANDLE     PortNumber;
  ///
  /// The protocol version number of the EAPOL implementation
  /// supported by the Port.
  ///
  UINT8               ProtocolVersion;
  ///
  /// The capabilities of the PAE associated with the Port. This field
  /// indicates whether Authenticator functionality, Supplicant
  /// functionality, both, or neither, is supported by the Port's PAE.
  ///
  UINT8               PaeCapabilities;
} EFI_EAPOL_PORT_INFO;

///
/// Supplicant PAE state machine (IEEE Std 802.1X Section 8.5.10)
///
typedef enum _EFI_EAPOL_SUPPLICANT_PAE_STATE {
  Logoff,
  Disconnected,
  Connecting,
  Acquired,
  Authenticating,
  Held,
  Authenticated,
  MaxSupplicantPaeState
} EFI_EAPOL_SUPPLICANT_PAE_STATE;

///
/// Definitions for ValidFieldMask
///
///@{
#define AUTH_PERIOD_FIELD_VALID       0x01
#define HELD_PERIOD_FIELD_VALID       0x02
#define START_PERIOD_FIELD_VALID      0x04
#define MAX_START_FIELD_VALID         0x08
///@}

///
/// EFI_EAPOL_SUPPLICANT_PAE_CONFIGURATION
///
typedef struct _EFI_EAPOL_SUPPLICANT_PAE_CONFIGURATION {
  ///
  /// Indicates which of the following fields are valid.
  ///
  UINT8       ValidFieldMask;
  ///
  /// The initial value for the authWhile timer. Its default value is 30s.
  ///
  UINTN       AuthPeriod;
  ///
  /// The initial value for the heldWhile timer. Its default value is 60s.
  ///
  UINTN       HeldPeriod;
  ///
  /// The initial value for the startWhen timer. Its default value is 30s.
  ///
  UINTN       StartPeriod;
  ///
  /// The maximum number of successive EAPOL-Start messages will
  /// be sent before the Supplicant assumes that there is no
  /// Authenticator present. Its default value is 3.
  ///
  UINTN       MaxStart;
} EFI_EAPOL_SUPPLICANT_PAE_CONFIGURATION;

///
/// Supplicant Statistics (IEEE Std 802.1X Section 9.5.2)
///
typedef struct _EFI_EAPOL_SUPPLICANT_PAE_STATISTICS {
  ///
  /// The number of EAPOL frames of any type that have been received by this Supplican.
  ///
  UINTN     EapolFramesReceived;
  ///
  /// The number of EAPOL frames of any type that have been transmitted by this Supplicant.
  ///
  UINTN     EapolFramesTransmitted;
  ///
  /// The number of EAPOL Start frames that have been transmitted by this Supplicant.
  ///
  UINTN     EapolStartFramesTransmitted;
  ///
  /// The number of EAPOL Logoff frames that have been transmitted by this Supplicant.
  ///
  UINTN     EapolLogoffFramesTransmitted;
  ///
  /// The number of EAP Resp/Id frames that have been transmitted by this Supplicant.
  ///
  UINTN     EapRespIdFramesTransmitted;
  ///
  /// The number of valid EAP Response frames (other than Resp/Id frames) that have been
  /// transmitted by this Supplicant.
  ///
  UINTN     EapResponseFramesTransmitted;
  ///
  /// The number of EAP Req/Id frames that have been received by this Supplicant.
  ///
  UINTN     EapReqIdFramesReceived;
  ///
  /// The number of EAP Request frames (other than Rq/Id frames) that have been received
  /// by this Supplicant.
  ///
  UINTN     EapRequestFramesReceived;
  ///
  /// The number of EAPOL frames that have been received by this Supplicant in which the
  /// frame type is not recognized.
  ///
  UINTN     InvalidEapolFramesReceived;
  ///
  /// The number of EAPOL frames that have been received by this Supplicant in which the
  /// Packet Body Length field (7.5.5) is invalid.
  ///
  UINTN     EapLengthErrorFramesReceived;
  ///
  /// The protocol version number carried in the most recently received EAPOL frame.
  ///
  UINTN     LastEapolFrameVersion;
  ///
  /// The source MAC address carried in the most recently received EAPOL frame.
  ///
  UINTN     LastEapolFrameSource;
} EFI_EAPOL_SUPPLICANT_PAE_STATISTICS;

/**
  Read the system configuration information associated with the Port.

  The GetSystemConfiguration() function reads the system configuration
  information associated with the Port, including the value of the
  SystemAuthControl parameter of the System is returned in SystemAuthControl
  and the Port's information is returned in the buffer pointed to by PortInfo.
  The Port's information is optional.
  If PortInfo is NULL, then reading the Port's information is ignored.

  If SystemAuthControl is NULL, then EFI_INVALID_PARAMETER is returned.

  @param[in]  This               A pointer to the EFI_EAP_MANAGEMENT_PROTOCOL
                                 instance that indicates the calling context.
  @param[out] SystemAuthControl  Returns the value of the SystemAuthControl
                                 parameter of the System.
                                 TRUE means Enabled. FALSE means Disabled.
  @param[out] PortInfo           Returns EFI_EAPOL_PORT_INFO structure to describe
                                 the Port's information. This parameter can be NULL
                                 to ignore reading the Port's information.

  @retval EFI_SUCCESS            The system configuration information of the
                                 Port is read successfully.
  @retval EFI_INVALID_PARAMETER  SystemAuthControl is NULL.


**/
typedef
EFI_STATUS
(EFIAPI *EFI_EAP_GET_SYSTEM_CONFIGURATION)(
  IN EFI_EAP_MANAGEMENT_PROTOCOL          *This,
  OUT BOOLEAN                             *SystemAuthControl,
  OUT EFI_EAPOL_PORT_INFO                 *PortInfo OPTIONAL
  );

/**
  Set the system configuration information associated with the Port.

  The SetSystemConfiguration() function sets the value of the SystemAuthControl
  parameter of the System to SystemAuthControl.

  @param[in] This                A pointer to the EFI_EAP_MANAGEMENT_PROTOCOL
                                 instance that indicates the calling context.
  @param[in] SystemAuthControl   The desired value of the SystemAuthControl
                                 parameter of the System.
                                 TRUE means Enabled. FALSE means Disabled.

  @retval EFI_SUCCESS            The system configuration information of the
                                 Port is set successfully.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_EAP_SET_SYSTEM_CONFIGURATION)(
  IN EFI_EAP_MANAGEMENT_PROTOCOL          *This,
  IN BOOLEAN                              SystemAuthControl
  );

/**
  Cause the EAPOL state machines for the Port to be initialized.

  The InitializePort() function causes the EAPOL state machines for the Port.

  @param[in] This                A pointer to the EFI_EAP_MANAGEMENT_PROTOCOL
                                 instance that indicates the calling context.

  @retval EFI_SUCCESS            The Port is initialized successfully.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_EAP_INITIALIZE_PORT)(
  IN EFI_EAP_MANAGEMENT_PROTOCOL            *This
  );

/**
  Notify the EAPOL state machines for the Port that the user of the System has
  logged on.

  The UserLogon() function notifies the EAPOL state machines for the Port.

  @param[in] This                A pointer to the EFI_EAP_MANAGEMENT_PROTOCOL
                                 instance that indicates the calling context.

  @retval EFI_SUCCESS            The Port is notified successfully.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_EAP_USER_LOGON)(
  IN EFI_EAP_MANAGEMENT_PROTOCOL          *This
  );

/**
  Notify the EAPOL state machines for the Port that the user of the System has
  logged off.

  The UserLogoff() function notifies the EAPOL state machines for the Port.

  @param[in] This                A pointer to the EFI_EAP_MANAGEMENT_PROTOCOL
                                 instance that indicates the calling context.

  @retval EFI_SUCCESS            The Port is notified successfully.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_EAP_USER_LOGOFF)(
  IN EFI_EAP_MANAGEMENT_PROTOCOL          *This
  );

/**
  Read the status of the Supplicant PAE state machine for the Port, including the
  current state and the configuration of the operational parameters.

  The GetSupplicantStatus() function reads the status of the Supplicant PAE state
  machine for the Port, including the current state CurrentState  and the configuration
  of the operational parameters Configuration. The configuration of the operational
  parameters is optional. If Configuration is NULL, then reading the configuration
  is ignored. The operational parameters in Configuration to be read can also be
  specified by Configuration.ValidFieldMask.

  If CurrentState is NULL, then EFI_INVALID_PARAMETER is returned.

  @param[in]      This           A pointer to the EFI_EAP_MANAGEMENT_PROTOCOL
                                 instance that indicates the calling context.
  @param[out]     CurrentState   Returns the current state of the Supplicant PAE
                                 state machine for the Port.
  @param[in, out] Configuration  Returns the configuration of the operational
                                 parameters of the Supplicant PAE state machine
                                 for the Port as required. This parameter can be
                                 NULL to ignore reading the configuration.
                                 On input, Configuration.ValidFieldMask specifies the
                                 operational parameters to be read.
                                 On output, Configuration returns the configuration
                                 of the required operational parameters.

  @retval EFI_SUCCESS            The configuration of the operational parameter
                                 of the Supplicant PAE state machine for the Port
                                 is set successfully.
  @retval EFI_INVALID_PARAMETER  CurrentState is NULL.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_EAP_GET_SUPPLICANT_STATUS)(
  IN EFI_EAP_MANAGEMENT_PROTOCOL                  *This,
  OUT EFI_EAPOL_SUPPLICANT_PAE_STATE              *CurrentState,
  IN OUT EFI_EAPOL_SUPPLICANT_PAE_CONFIGURATION   *Configuration  OPTIONAL
  );

/**
  Set the configuration of the operational parameter of the Supplicant PAE
  state machine for the Port.

  The SetSupplicantConfiguration() function sets the configuration of the
  operational Parameter of the Supplicant PAE state machine for the Port to
  Configuration. The operational parameters in Configuration to be set can be
  specified by Configuration.ValidFieldMask.

  If Configuration is NULL, then EFI_INVALID_PARAMETER is returned.

  @param[in] This                A pointer to the EFI_EAP_MANAGEMENT_PROTOCOL
                                 instance that indicates the calling context.
  @param[in] Configuration       The desired configuration of the operational
                                 parameters of the Supplicant PAE state machine
                                 for the Port as required.

  @retval EFI_SUCCESS            The configuration of the operational parameter
                                 of the Supplicant PAE state machine for the Port
                                 is set successfully.
  @retval EFI_INVALID_PARAMETER  Configuration is NULL.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_EAP_SET_SUPPLICANT_CONFIGURATION)(
  IN EFI_EAP_MANAGEMENT_PROTOCOL              *This,
  IN EFI_EAPOL_SUPPLICANT_PAE_CONFIGURATION   *Configuration
  );

/**
  Read the statistical information regarding the operation of the Supplicant
  associated with the Port.

  The GetSupplicantStatistics() function reads the statistical information
  Statistics regarding the operation of the Supplicant associated with the Port.

  If Statistics is NULL, then EFI_INVALID_PARAMETER is returned.

  @param[in]  This               A pointer to the EFI_EAP_MANAGEMENT_PROTOCOL
                                 instance that indicates the calling context.
  @param[out] Statistics         Returns the statistical information regarding the
                                 operation of the Supplicant for the Port.

  @retval EFI_SUCCESS            The statistical information regarding the operation
                                 of the Supplicant for the Port is read successfully.
  @retval EFI_INVALID_PARAMETER  Statistics is NULL.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_EAP_GET_SUPPLICANT_STATISTICS)(
  IN EFI_EAP_MANAGEMENT_PROTOCOL              *This,
  OUT EFI_EAPOL_SUPPLICANT_PAE_STATISTICS     *Statistics
  );

///
/// EFI_EAP_MANAGEMENT_PROTOCOL
/// is used to control, configure and monitor EAPOL state machine on
/// a Port. EAPOL state machine is built on a per-Port basis. Herein,
/// a Port means a NIC. For the details of EAPOL, please refer to
/// IEEE 802.1x specification.
///
struct _EFI_EAP_MANAGEMENT_PROTOCOL {
  EFI_EAP_GET_SYSTEM_CONFIGURATION        GetSystemConfiguration;
  EFI_EAP_SET_SYSTEM_CONFIGURATION        SetSystemConfiguration;
  EFI_EAP_INITIALIZE_PORT                 InitializePort;
  EFI_EAP_USER_LOGON                      UserLogon;
  EFI_EAP_USER_LOGOFF                     UserLogoff;
  EFI_EAP_GET_SUPPLICANT_STATUS           GetSupplicantStatus;
  EFI_EAP_SET_SUPPLICANT_CONFIGURATION    SetSupplicantConfiguration;
  EFI_EAP_GET_SUPPLICANT_STATISTICS       GetSupplicantStatistics;
};

extern EFI_GUID gEfiEapManagementProtocolGuid;

#endif

