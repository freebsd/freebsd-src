/** @file
  I2C Bus Configuration Management Protocol as defined in the PI 1.3 specification.

  The EFI I2C bus configuration management protocol provides platform specific
  services that allow the I2C host protocol to reconfigure the switches and multiplexers
  and set the clock frequency for the I2C bus. This protocol also enables the I2C host protocol
  to reset an I2C device which may be locking up the I2C bus by holding the clock or data line low.

  Copyright (c) 2013 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This protocol is from PI Version 1.3.

**/

#ifndef __I2C_BUS_CONFIGURATION_MANAGEMENT_H__
#define __I2C_BUS_CONFIGURATION_MANAGEMENT_H__

#define EFI_I2C_BUS_CONFIGURATION_MANAGEMENT_PROTOCOL_GUID \
  { 0x55b71fb5, 0x17c6, 0x410e, { 0xb5, 0xbd, 0x5f, 0xa2, 0xe3, 0xd4, 0x46, 0x6b }}

///
/// I2C bus configuration management protocol
///
/// The EFI I2C bus configuration management protocol provides platform
/// specific services that allow the I2C host protocol to reconfigure the
/// switches and multiplexers and set the clock frequency for the I2C bus.
/// This protocol also enables the I2C host protocol to reset an I2C device
/// which may be locking up the I2C bus by holding the clock or data line
/// low.
///
/// The I2C protocol stack uses the concept of an I2C bus configuration as
/// a way to describe a particular state of the switches and multiplexers
/// in the I2C bus.
///
/// A simple I2C bus does not have any multiplexers or switches is described
/// to the I2C protocol stack with a single I2C bus configuration which
/// specifies the I2C bus frequency.
///
/// An I2C bus with switches and multiplexers use an I2C bus configuration
/// to describe each of the unique settings for the switches and multiplexers
/// and the I2C bus frequency.  However the I2C bus configuration management
/// protocol only needs to define the I2C bus configurations that the software
/// uses, which may be a subset of the total.
///
/// The I2C bus configuration description includes a list of I2C devices
/// which may be accessed when this I2C bus configuration is enabled.  I2C
/// devices before a switch or multiplexer must be included in one I2C bus
/// configuration while I2C devices after a switch or multiplexer are on
/// another I2C bus configuration.
///
/// The I2C bus configuration management protocol is an optional protocol.
/// When the I2C bus configuration protocol is not defined the I2C host
/// protocol does not start and the I2C master protocol may be used for
/// other purposes such as SMBus traffic.  When the I2C bus configuration
/// protocol is available, the I2C host protocol uses the I2C bus
/// configuration protocol to call into the platform specific code to set
/// the switches and multiplexers and set the maximum I2C bus frequency.
///
/// The platform designers determine the maximum I2C bus frequency by
/// selecting a frequency which supports all of the I2C devices on the
/// I2C bus for the setting of switches and multiplexers.  The platform
/// designers must validate this against the I2C device data sheets and
/// any limits of the I2C controller or bus length.
///
/// During I2C device enumeration, the I2C bus driver retrieves the I2C
/// bus configuration that must be used to perform I2C transactions to
/// each I2C device.  This I2C bus configuration value is passed into
/// the I2C host protocol to identify the I2C bus configuration required
/// to access a specific I2C device.  The I2C host protocol calls
/// EnableBusConfiguration() to set the switches and multiplexers in the
/// I2C bus and the I2C clock frequency.  The I2C host protocol may
/// optimize calls to EnableBusConfiguration() by only making the call
/// when the I2C bus configuration value changes between I2C requests.
///
/// When I2C transactions are required on the same I2C bus to change the
/// state of multiplexers or switches, the I2C master protocol must be
/// used to perform the necessary I2C transactions.
///
/// It is up to the platform specific code to choose the proper I2C bus
/// configuration when ExitBootServices() is called. Some operating systems
/// are not able to manage the I2C bus configurations and must use the I2C
/// bus configuration that is established by the platform firmware before
/// ExitBootServices() returns.
///
typedef struct _EFI_I2C_BUS_CONFIGURATION_MANAGEMENT_PROTOCOL EFI_I2C_BUS_CONFIGURATION_MANAGEMENT_PROTOCOL;


/**
  Enable access to an I2C bus configuration.

  This routine must be called at or below TPL_NOTIFY.  For synchronous
  requests this routine must be called at or below TPL_CALLBACK.

  Reconfigure the switches and multiplexers in the I2C bus to enable
  access to a specific I2C bus configuration.  Also select the maximum
  clock frequency for this I2C bus configuration.

  This routine uses the I2C Master protocol to perform I2C transactions
  on the local bus.  This eliminates any recursion in the I2C stack for
  configuration transactions on the same I2C bus.  This works because the
  local I2C bus is idle while the I2C bus configuration is being enabled.

  If I2C transactions must be performed on other I2C busses, then the
  EFI_I2C_HOST_PROTOCOL, the EFI_I2C_IO_PROTCOL, or a third party I2C
  driver interface for a specific device must be used.  This requirement
  is because the I2C host protocol controls the flow of requests to the
  I2C controller.  Use the EFI_I2C_HOST_PROTOCOL when the I2C device is
  not enumerated by the EFI_I2C_ENUMERATE_PROTOCOL.  Use a protocol
  produced by a third party driver when it is available or the
  EFI_I2C_IO_PROTOCOL when the third party driver is not available but
  the device is enumerated with the EFI_I2C_ENUMERATE_PROTOCOL.

  When Event is NULL, EnableI2cBusConfiguration operates synchronously
  and returns the I2C completion status as its return value.

  @param[in]  This            Pointer to an EFI_I2C_BUS_CONFIGURATION_MANAGEMENT_PROTOCOL
                              structure.
  @param[in]  I2cBusConfiguration Index of an I2C bus configuration.  All
                                  values in the range of zero to N-1 are
                                  valid where N is the total number of I2C
                                  bus configurations for an I2C bus.
  @param[in]  Event           Event to signal when the transaction is complete
  @param[out] I2cStatus       Buffer to receive the transaction status.

  @return  When Event is NULL, EnableI2cBusConfiguration operates synchrouously
  and returns the I2C completion status as its return value.  In this case it is
  recommended to use NULL for I2cStatus.  The values returned from
  EnableI2cBusConfiguration are:

  @retval EFI_SUCCESS           The asynchronous bus configuration request
                                was successfully started when Event is not
                                NULL.
  @retval EFI_SUCCESS           The bus configuration request completed
                                successfully when Event is NULL.
  @retval EFI_DEVICE_ERROR      The bus configuration failed.
  @retval EFI_NO_MAPPING        Invalid I2cBusConfiguration value

**/
typedef
EFI_STATUS
(EFIAPI *EFI_I2C_BUS_CONFIGURATION_MANAGEMENT_PROTOCOL_ENABLE_I2C_BUS_CONFIGURATION) (
  IN CONST EFI_I2C_BUS_CONFIGURATION_MANAGEMENT_PROTOCOL *This,
  IN UINTN                                               I2cBusConfiguration,
  IN EFI_EVENT                                           Event      OPTIONAL,
  IN EFI_STATUS                                          *I2cStatus OPTIONAL
  );

///
/// I2C bus configuration management protocol
///
struct _EFI_I2C_BUS_CONFIGURATION_MANAGEMENT_PROTOCOL {
  ///
  /// Enable an I2C bus configuration for use.
  ///
  EFI_I2C_BUS_CONFIGURATION_MANAGEMENT_PROTOCOL_ENABLE_I2C_BUS_CONFIGURATION EnableI2cBusConfiguration;
};

///
/// Reference to variable defined in the .DEC file
///
extern EFI_GUID gEfiI2cBusConfigurationManagementProtocolGuid;

#endif  //  __I2C_BUS_CONFIGURATION_MANAGEMENT_H__
