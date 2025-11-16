/** @file
  UEFI Driver Configuration2 Protocol

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __EFI_DRIVER_CONFIGURATION2_H__
#define __EFI_DRIVER_CONFIGURATION2_H__

///
/// Global ID for the Driver Configuration Protocol defined in UEFI 2.0
///
#define EFI_DRIVER_CONFIGURATION2_PROTOCOL_GUID \
  { \
    0xbfd7dc1d, 0x24f1, 0x40d9, {0x82, 0xe7, 0x2e, 0x09, 0xbb, 0x6b, 0x4e, 0xbe } \
  }

typedef struct _EFI_DRIVER_CONFIGURATION2_PROTOCOL EFI_DRIVER_CONFIGURATION2_PROTOCOL;

typedef enum {
  ///
  /// The controller is still in a usable state. No actions
  /// are required before this controller can be used again.
  ///
  EfiDriverConfigurationActionNone = 0,
  ///
  /// The driver has detected that the controller is not in a
  /// usable state, and it needs to be stopped.
  ///
  EfiDriverConfigurationActionStopController = 1,
  ///
  /// This controller needs to be stopped and restarted
  /// before it can be used again.
  ///
  EfiDriverConfigurationActionRestartController = 2,
  ///
  /// A configuration change has been made that requires the platform to be restarted before
  /// the controller can be used again.
  ///
  EfiDriverConfigurationActionRestartPlatform = 3,
  EfiDriverConfigurationActionMaximum
} EFI_DRIVER_CONFIGURATION_ACTION_REQUIRED;

#define EFI_DRIVER_CONFIGURATION_SAFE_DEFAULTS           0x00000000
#define EFI_DRIVER_CONFIGURATION_MANUFACTURING_DEFAULTS  0x00000001
#define EFI_DRIVER_CONFIGURATION_CUSTOM_DEFAULTS         0x00000002
#define EFI_DRIVER_CONFIGURATION_PERORMANCE_DEFAULTS     0x00000003

/**
  Allows the user to set controller specific options for a controller that a
  driver is currently managing.

  @param  This             A pointer to the EFI_DRIVER_CONFIGURATION2_PROTOCOL instance.
  @param  ControllerHandle The handle of the controller to set options on.
  @param  ChildHandle      The handle of the child controller to set options on.  This
                           is an optional parameter that may be NULL.  It will be NULL
                           for device drivers, and for bus drivers that wish to set
                           options for the bus controller.  It will not be NULL for a
                           bus driver that wishes to set options for one of its child
                           controllers.
  @param  Language         A Null-terminated ASCII string that contains one or more RFC 4646
                           language codes. This is the list of language codes that this
                           protocol supports. The number of languages
                           supported by a driver is up to the driver writer.
  @param  ActionRequired   A pointer to the action that the calling agent is required
                           to perform when this function returns.  See "Related
                           Definitions" for a list of the actions that the calling
                           agent is required to perform prior to accessing
                           ControllerHandle again.

  @retval EFI_SUCCESS           The driver specified by This successfully set the
                                configuration options for the controller specified
                                by ControllerHandle.
  @retval EFI_INVALID_PARAMETER ControllerHandle is not a valid EFI_HANDLE.
  @retval EFI_INVALID_PARAMETER ChildHandle is not NULL and it is not a valid EFI_HANDLE.
  @retval EFI_INVALID_PARAMETER ActionRequired is NULL.
  @retval EFI_UNSUPPORTED       The driver specified by This does not support setting
                                configuration options for the controller specified by
                                ControllerHandle and ChildHandle.
  @retval EFI_UNSUPPORTED       The driver specified by This does not support the
                                language specified by Language.
  @retval EFI_DEVICE_ERROR      A device error occurred while attempting to set the
                                configuration options for the controller specified
                                by ControllerHandle and ChildHandle.
  @retval EFI_OUT_RESOURCES     There are not enough resources available to set the
                                configuration options for the controller specified
                                by ControllerHandle and ChildHandle.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_DRIVER_CONFIGURATION2_SET_OPTIONS)(
  IN EFI_DRIVER_CONFIGURATION2_PROTOCOL                       *This,
  IN  EFI_HANDLE                                              ControllerHandle,
  IN  EFI_HANDLE                                              ChildHandle  OPTIONAL,
  IN  CHAR8                                                   *Language,
  OUT EFI_DRIVER_CONFIGURATION_ACTION_REQUIRED                *ActionRequired
  );

/**
  Tests to see if a controller's current configuration options are valid.

  @param  This             A pointer to the EFI_DRIVER_CONFIGURATION2_PROTOCOL instance.
  @param  ControllerHandle The handle of the controller to test if it's current
                           configuration options are valid.
  @param  ChildHandle      The handle of the child controller to test if it's current
                           configuration options are valid.  This is an optional
                           parameter that may be NULL.  It will be NULL for device
                           drivers.  It will also be NULL for bus drivers that wish
                           to test the configuration options for the bus controller.
                           It will not be NULL for a bus driver that wishes to test
                           configuration options for one of its child controllers.

  @retval EFI_SUCCESS           The controller specified by ControllerHandle and
                                ChildHandle that is being managed by the driver
                                specified by This has a valid set of  configuration
                                options.
  @retval EFI_INVALID_PARAMETER ControllerHandle is not a valid EFI_HANDLE.
  @retval EFI_INVALID_PARAMETER ChildHandle is not NULL and it is not a valid EFI_HANDLE.
  @retval EFI_UNSUPPORTED       The driver specified by This is not currently
                                managing the controller specified by ControllerHandle
                                and ChildHandle.
  @retval EFI_DEVICE_ERROR      The controller specified by ControllerHandle and
                                ChildHandle that is being managed by the driver
                                specified by This has an invalid set of configuration
                                options.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_DRIVER_CONFIGURATION2_OPTIONS_VALID)(
  IN EFI_DRIVER_CONFIGURATION2_PROTOCOL                       *This,
  IN  EFI_HANDLE                                              ControllerHandle,
  IN  EFI_HANDLE                                              ChildHandle  OPTIONAL
  );

/**
  Forces a driver to set the default configuration options for a controller.

  @param  This             A pointer to the EFI_DRIVER_CONFIGURATION2_PROTOCOL instance.
  @param  ControllerHandle The handle of the controller to force default configuration options on.
  @param  ChildHandle      The handle of the child controller to force default configuration options on  This is an optional parameter that may be NULL.  It will be NULL for device drivers.  It will also be NULL for bus drivers that wish to force default configuration options for the bus controller.  It will not be NULL for a bus driver that wishes to force default configuration options for one of its child controllers.
  @param  DefaultType      The type of default configuration options to force on the controller specified by ControllerHandle and ChildHandle.  See Table 9-1 for legal values.  A DefaultType of 0x00000000 must be supported by this protocol.
  @param  ActionRequired   A pointer to the action that the calling agent is required to perform when this function returns.  See "Related Definitions" in Section 9.1 for a list of the actions that the calling agent is required to perform prior to accessing ControllerHandle again.

  @retval EFI_SUCCESS           The driver specified by This successfully forced the default configuration options on the controller specified by ControllerHandle and ChildHandle.
  @retval EFI_INVALID_PARAMETER ControllerHandle is not a valid EFI_HANDLE.
  @retval EFI_INVALID_PARAMETER ChildHandle is not NULL and it is not a valid EFI_HANDLE.
  @retval EFI_INVALID_PARAMETER ActionRequired is NULL.
  @retval EFI_UNSUPPORTED       The driver specified by This does not support forcing the default configuration options on the controller specified by ControllerHandle and ChildHandle.
  @retval EFI_UNSUPPORTED       The driver specified by This does not support the configuration type specified by DefaultType.
  @retval EFI_DEVICE_ERROR      A device error occurred while attempt to force the default configuration options on the controller specified by  ControllerHandle and ChildHandle.
  @retval EFI_OUT_RESOURCES     There are not enough resources available to force the default configuration options on the controller specified by ControllerHandle and ChildHandle.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_DRIVER_CONFIGURATION2_FORCE_DEFAULTS)(
  IN EFI_DRIVER_CONFIGURATION2_PROTOCOL                        *This,
  IN  EFI_HANDLE                                              ControllerHandle,
  IN  EFI_HANDLE                                              ChildHandle  OPTIONAL,
  IN  UINT32                                                  DefaultType,
  OUT EFI_DRIVER_CONFIGURATION_ACTION_REQUIRED                *ActionRequired
  );

///
/// Used to set configuration options for a controller that an EFI Driver is managing.
///
struct _EFI_DRIVER_CONFIGURATION2_PROTOCOL {
  EFI_DRIVER_CONFIGURATION2_SET_OPTIONS       SetOptions;
  EFI_DRIVER_CONFIGURATION2_OPTIONS_VALID     OptionsValid;
  EFI_DRIVER_CONFIGURATION2_FORCE_DEFAULTS    ForceDefaults;
  ///
  /// A Null-terminated ASCII string that contains one or more RFC 4646
  /// language codes.  This is the list of language codes that this protocol supports.
  ///
  CHAR8                                       *SupportedLanguages;
};

extern EFI_GUID  gEfiDriverConfiguration2ProtocolGuid;

#endif
