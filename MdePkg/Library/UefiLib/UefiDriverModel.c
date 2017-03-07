/** @file
  Library functions that abstract driver model protocols
  installation.

  Copyright (c) 2006 - 2013, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials are
  licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.
  
  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/ 


#include "UefiLibInternal.h"

/**
  Installs and completes the initialization of a Driver Binding Protocol instance.
  
  Installs the Driver Binding Protocol specified by DriverBinding onto the handle
  specified by DriverBindingHandle. If DriverBindingHandle is NULL, then DriverBinding
  is installed onto a newly created handle. DriverBindingHandle is typically the same
  as the driver's ImageHandle, but it can be different if the driver produces multiple
  Driver Binding Protocols. 
  If DriverBinding is NULL, then ASSERT(). 
  If DriverBinding can not be installed onto a handle, then ASSERT().

  @param  ImageHandle          The image handle of the driver.
  @param  SystemTable          The EFI System Table that was passed to the driver's entry point.
  @param  DriverBinding        A Driver Binding Protocol instance that this driver is producing.
  @param  DriverBindingHandle  The handle that DriverBinding is to be installed onto.  If this
                               parameter is NULL, then a new handle is created.

  @retval EFI_SUCCESS           The protocol installation successfully completed.
  @retval EFI_OUT_OF_RESOURCES  There was not enough system resources to install the protocol.
  @retval Others                Status from gBS->InstallMultipleProtocolInterfaces().

**/
EFI_STATUS
EFIAPI
EfiLibInstallDriverBinding (
  IN CONST EFI_HANDLE             ImageHandle,
  IN CONST EFI_SYSTEM_TABLE       *SystemTable,
  IN EFI_DRIVER_BINDING_PROTOCOL  *DriverBinding,
  IN EFI_HANDLE                   DriverBindingHandle
  )
{
  EFI_STATUS  Status;

  ASSERT (DriverBinding != NULL);

  //
  // Update the ImageHandle and DriverBindingHandle fields of the Driver Binding Protocol
  //
  DriverBinding->ImageHandle         = ImageHandle;
  DriverBinding->DriverBindingHandle = DriverBindingHandle;

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &DriverBinding->DriverBindingHandle,
                  &gEfiDriverBindingProtocolGuid, DriverBinding,
                  NULL
                  );
  //
  // ASSERT if the call to InstallMultipleProtocolInterfaces() failed
  //
  ASSERT_EFI_ERROR (Status);

  return Status;
}


/**
  Installs and completes the initialization of a Driver Binding Protocol instance and
  optionally installs the Component Name, Driver Configuration and Driver Diagnostics Protocols.

  Initializes a driver by installing the Driver Binding Protocol together with the
  optional Component Name, optional Driver Configure and optional Driver Diagnostic
  Protocols onto the driver's DriverBindingHandle. If DriverBindingHandle is NULL,
  then the protocols are  installed onto a newly created handle. DriverBindingHandle
  is typically the same as the driver's ImageHandle, but it can be different if the
  driver produces multiple Driver Binding Protocols. 
  If DriverBinding is NULL, then ASSERT(). 
  If the installation fails, then ASSERT().
  
  @param  ImageHandle          The image handle of the driver.
  @param  SystemTable          The EFI System Table that was passed to the driver's entry point.
  @param  DriverBinding        A Driver Binding Protocol instance that this driver is producing.
  @param  DriverBindingHandle  The handle that DriverBinding is to be installed onto.  If this
                               parameter is NULL, then a new handle is created.
  @param  ComponentName        A Component Name Protocol instance that this driver is producing.
  @param  DriverConfiguration  A Driver Configuration Protocol instance that this driver is producing.
  @param  DriverDiagnostics    A Driver Diagnostics Protocol instance that this driver is producing.

  @retval EFI_SUCCESS           The protocol installation successfully completed.
  @retval EFI_OUT_OF_RESOURCES  There was not enough memory in pool to install all the protocols.

**/
EFI_STATUS
EFIAPI
EfiLibInstallAllDriverProtocols (
  IN CONST EFI_HANDLE                         ImageHandle,
  IN CONST EFI_SYSTEM_TABLE                   *SystemTable,
  IN EFI_DRIVER_BINDING_PROTOCOL              *DriverBinding,
  IN EFI_HANDLE                               DriverBindingHandle,
  IN CONST EFI_COMPONENT_NAME_PROTOCOL        *ComponentName,       OPTIONAL
  IN CONST EFI_DRIVER_CONFIGURATION_PROTOCOL  *DriverConfiguration, OPTIONAL
  IN CONST EFI_DRIVER_DIAGNOSTICS_PROTOCOL    *DriverDiagnostics    OPTIONAL
  )
{
  EFI_STATUS  Status;

  ASSERT (DriverBinding != NULL);

  //
  // Update the ImageHandle and DriverBindingHandle fields of the Driver Binding Protocol
  //
  DriverBinding->ImageHandle         = ImageHandle;
  DriverBinding->DriverBindingHandle = DriverBindingHandle;
  
  if (DriverDiagnostics == NULL || FeaturePcdGet(PcdDriverDiagnosticsDisable)) {
    if (DriverConfiguration == NULL) {
      if (ComponentName == NULL || FeaturePcdGet(PcdComponentNameDisable)) {
        Status = gBS->InstallMultipleProtocolInterfaces (
                        &DriverBinding->DriverBindingHandle,
                        &gEfiDriverBindingProtocolGuid, DriverBinding,
                        NULL
                        );
      } else {
        Status = gBS->InstallMultipleProtocolInterfaces (
                        &DriverBinding->DriverBindingHandle,
                        &gEfiDriverBindingProtocolGuid, DriverBinding,
                        &gEfiComponentNameProtocolGuid, ComponentName,
                        NULL
                        );
      }
    } else {
      if (ComponentName == NULL || FeaturePcdGet(PcdComponentNameDisable)) {
        Status = gBS->InstallMultipleProtocolInterfaces (
                        &DriverBinding->DriverBindingHandle,
                        &gEfiDriverBindingProtocolGuid,       DriverBinding,
                        &gEfiDriverConfigurationProtocolGuid, DriverConfiguration,
                        NULL
                        );
      } else {
        Status = gBS->InstallMultipleProtocolInterfaces (
                        &DriverBinding->DriverBindingHandle,
                        &gEfiDriverBindingProtocolGuid,       DriverBinding,
                        &gEfiComponentNameProtocolGuid,       ComponentName,
                        &gEfiDriverConfigurationProtocolGuid, DriverConfiguration,
                        NULL
                        );
      }
    }
  } else {
    if (DriverConfiguration == NULL) {
      if (ComponentName == NULL || FeaturePcdGet(PcdComponentNameDisable)) {
        Status = gBS->InstallMultipleProtocolInterfaces (
                        &DriverBinding->DriverBindingHandle,
                        &gEfiDriverBindingProtocolGuid,     DriverBinding,
                        &gEfiDriverDiagnosticsProtocolGuid, DriverDiagnostics,
                        NULL
                        );
      } else {
        Status = gBS->InstallMultipleProtocolInterfaces (
                        &DriverBinding->DriverBindingHandle,
                        &gEfiDriverBindingProtocolGuid,     DriverBinding,
                        &gEfiComponentNameProtocolGuid,     ComponentName,
                        &gEfiDriverDiagnosticsProtocolGuid, DriverDiagnostics,
                        NULL
                        );
      }
    } else {
      if (ComponentName == NULL || FeaturePcdGet(PcdComponentNameDisable)) {
       Status = gBS->InstallMultipleProtocolInterfaces (
                        &DriverBinding->DriverBindingHandle,
                        &gEfiDriverBindingProtocolGuid,       DriverBinding,
                        &gEfiDriverConfigurationProtocolGuid, DriverConfiguration,
                        &gEfiDriverDiagnosticsProtocolGuid,   DriverDiagnostics,
                        NULL
                        );
      } else {
        Status = gBS->InstallMultipleProtocolInterfaces (
                        &DriverBinding->DriverBindingHandle,
                        &gEfiDriverBindingProtocolGuid,       DriverBinding,
                        &gEfiComponentNameProtocolGuid,       ComponentName,
                        &gEfiDriverConfigurationProtocolGuid, DriverConfiguration,
                        &gEfiDriverDiagnosticsProtocolGuid,   DriverDiagnostics,
                        NULL
                        );
      }
    }
  }

  //
  // ASSERT if the call to InstallMultipleProtocolInterfaces() failed
  //
  ASSERT_EFI_ERROR (Status);

  return Status;
}



/**
  Installs Driver Binding Protocol with optional Component Name and Component Name 2 Protocols.

  Initializes a driver by installing the Driver Binding Protocol together with the
  optional Component Name and optional Component Name 2 protocols onto the driver's
  DriverBindingHandle.  If DriverBindingHandle is NULL, then the protocols are installed
  onto a newly created handle.  DriverBindingHandle is typically the same as the driver's
  ImageHandle, but it can be different if the driver produces multiple Driver Binding Protocols. 
  If DriverBinding is NULL, then ASSERT(). 
  If the installation fails, then ASSERT().

  @param  ImageHandle          The image handle of the driver.
  @param  SystemTable          The EFI System Table that was passed to the driver's entry point.
  @param  DriverBinding        A Driver Binding Protocol instance that this driver is producing.
  @param  DriverBindingHandle  The handle that DriverBinding is to be installed onto.  If this
                               parameter is NULL, then a new handle is created.
  @param  ComponentName        A Component Name Protocol instance that this driver is producing.
  @param  ComponentName2       A Component Name 2 Protocol instance that this driver is producing.

  @retval EFI_SUCCESS           The protocol installation successfully completed.
  @retval EFI_OUT_OF_RESOURCES  There was not enough memory in pool to install all the protocols.

**/
EFI_STATUS
EFIAPI
EfiLibInstallDriverBindingComponentName2 (
  IN CONST EFI_HANDLE                         ImageHandle,
  IN CONST EFI_SYSTEM_TABLE                   *SystemTable,
  IN EFI_DRIVER_BINDING_PROTOCOL              *DriverBinding,
  IN EFI_HANDLE                               DriverBindingHandle,
  IN CONST EFI_COMPONENT_NAME_PROTOCOL        *ComponentName,       OPTIONAL
  IN CONST EFI_COMPONENT_NAME2_PROTOCOL       *ComponentName2       OPTIONAL
  )
{
  EFI_STATUS  Status;

  ASSERT (DriverBinding != NULL);

  //
  // Update the ImageHandle and DriverBindingHandle fields of the Driver Binding Protocol
  //
  DriverBinding->ImageHandle         = ImageHandle;
  DriverBinding->DriverBindingHandle = DriverBindingHandle;

  if (ComponentName == NULL || FeaturePcdGet(PcdComponentNameDisable)) {
    if (ComponentName2 == NULL || FeaturePcdGet(PcdComponentName2Disable)) {
      Status = gBS->InstallMultipleProtocolInterfaces (
                      &DriverBinding->DriverBindingHandle,
                      &gEfiDriverBindingProtocolGuid, DriverBinding,
                      NULL
                      );
      } else {
      Status = gBS->InstallMultipleProtocolInterfaces (
                      &DriverBinding->DriverBindingHandle,
                      &gEfiDriverBindingProtocolGuid, DriverBinding,
                      &gEfiComponentName2ProtocolGuid, ComponentName2,
                      NULL
                      );
     }
  } else {
     if (ComponentName2 == NULL || FeaturePcdGet(PcdComponentName2Disable)) {
       Status = gBS->InstallMultipleProtocolInterfaces (
                       &DriverBinding->DriverBindingHandle,
                       &gEfiDriverBindingProtocolGuid, DriverBinding,
                       &gEfiComponentNameProtocolGuid, ComponentName,
                       NULL
                       );
     } else {
       Status = gBS->InstallMultipleProtocolInterfaces (
                       &DriverBinding->DriverBindingHandle,
                       &gEfiDriverBindingProtocolGuid, DriverBinding,
                       &gEfiComponentNameProtocolGuid, ComponentName,
                       &gEfiComponentName2ProtocolGuid, ComponentName2,
                       NULL
                       );
    }
  }

  //
  // ASSERT if the call to InstallMultipleProtocolInterfaces() failed
  //
  ASSERT_EFI_ERROR (Status);

  return Status;
}



/**
  Installs Driver Binding Protocol with optional Component Name, Component Name 2, Driver
  Configuration, Driver Configuration 2, Driver Diagnostics, and Driver Diagnostics 2 Protocols.

  Initializes a driver by installing the Driver Binding Protocol together with the optional
  Component Name, optional Component Name 2, optional Driver Configuration, optional Driver Configuration 2,
  optional Driver Diagnostic, and optional Driver Diagnostic 2 Protocols onto the driver's DriverBindingHandle.
  DriverBindingHandle is typically the same as the driver's ImageHandle, but it can be different if the driver
  produces multiple Driver Binding Protocols. 
  If DriverBinding is NULL, then ASSERT(). 
  If the installation fails, then ASSERT().


  @param  ImageHandle           The image handle of the driver.
  @param  SystemTable           The EFI System Table that was passed to the driver's entry point.
  @param  DriverBinding         A Driver Binding Protocol instance that this driver is producing.
  @param  DriverBindingHandle   The handle that DriverBinding is to be installed onto.  If this
                                parameter is NULL, then a new handle is created.
  @param  ComponentName         A Component Name Protocol instance that this driver is producing.
  @param  ComponentName2        A Component Name 2 Protocol instance that this driver is producing.
  @param  DriverConfiguration   A Driver Configuration Protocol instance that this driver is producing.
  @param  DriverConfiguration2  A Driver Configuration Protocol 2 instance that this driver is producing.
  @param  DriverDiagnostics     A Driver Diagnostics Protocol instance that this driver is producing.
  @param  DriverDiagnostics2    A Driver Diagnostics Protocol 2 instance that this driver is producing.

  @retval EFI_SUCCESS           The protocol installation successfully completed.
  @retval EFI_OUT_OF_RESOURCES  There was not enough memory in pool to install all the protocols.

**/
EFI_STATUS
EFIAPI
EfiLibInstallAllDriverProtocols2 (
  IN CONST EFI_HANDLE                         ImageHandle,
  IN CONST EFI_SYSTEM_TABLE                   *SystemTable,
  IN EFI_DRIVER_BINDING_PROTOCOL              *DriverBinding,
  IN EFI_HANDLE                               DriverBindingHandle,
  IN CONST EFI_COMPONENT_NAME_PROTOCOL        *ComponentName,        OPTIONAL
  IN CONST EFI_COMPONENT_NAME2_PROTOCOL       *ComponentName2,       OPTIONAL
  IN CONST EFI_DRIVER_CONFIGURATION_PROTOCOL  *DriverConfiguration,  OPTIONAL
  IN CONST EFI_DRIVER_CONFIGURATION2_PROTOCOL *DriverConfiguration2, OPTIONAL
  IN CONST EFI_DRIVER_DIAGNOSTICS_PROTOCOL    *DriverDiagnostics,    OPTIONAL
  IN CONST EFI_DRIVER_DIAGNOSTICS2_PROTOCOL   *DriverDiagnostics2    OPTIONAL
  )
{
  EFI_STATUS  Status;

  ASSERT (DriverBinding != NULL); 

  //
  // Update the ImageHandle and DriverBindingHandle fields of the Driver Binding Protocol
  //
  DriverBinding->ImageHandle         = ImageHandle;
  DriverBinding->DriverBindingHandle = DriverBindingHandle;
  
  if (DriverConfiguration2 == NULL) {
    if (DriverConfiguration == NULL) {
      if (DriverDiagnostics == NULL || FeaturePcdGet(PcdDriverDiagnosticsDisable)) {
        if (DriverDiagnostics2 == NULL || FeaturePcdGet(PcdDriverDiagnostics2Disable)) {
          if (ComponentName == NULL || FeaturePcdGet(PcdComponentNameDisable)) {
            if (ComponentName2 == NULL || FeaturePcdGet(PcdComponentName2Disable)) {
              Status = gBS->InstallMultipleProtocolInterfaces (
                              &DriverBinding->DriverBindingHandle,
                              &gEfiDriverBindingProtocolGuid, DriverBinding,
                              NULL
                              );
            } else {
              Status = gBS->InstallMultipleProtocolInterfaces (
                              &DriverBinding->DriverBindingHandle,
                              &gEfiDriverBindingProtocolGuid, DriverBinding,
                              &gEfiComponentName2ProtocolGuid, ComponentName2,
                              NULL
                              );
            }
          } else {
            if (ComponentName2 == NULL || FeaturePcdGet(PcdComponentName2Disable)) {
              Status = gBS->InstallMultipleProtocolInterfaces (
                              &DriverBinding->DriverBindingHandle,
                              &gEfiDriverBindingProtocolGuid, DriverBinding,
                              &gEfiComponentNameProtocolGuid, ComponentName,
                              NULL
                              );
            } else {
              Status = gBS->InstallMultipleProtocolInterfaces (
                              &DriverBinding->DriverBindingHandle,
                              &gEfiDriverBindingProtocolGuid, DriverBinding,
                              &gEfiComponentNameProtocolGuid, ComponentName,
                              &gEfiComponentName2ProtocolGuid, ComponentName2,
                              NULL
                              );
            }
          }
        } else {
          if (ComponentName == NULL || FeaturePcdGet(PcdComponentNameDisable)) {
            if (ComponentName2 == NULL || FeaturePcdGet(PcdComponentName2Disable)) {
              Status = gBS->InstallMultipleProtocolInterfaces (
                              &DriverBinding->DriverBindingHandle,
                              &gEfiDriverBindingProtocolGuid, DriverBinding,
                              &gEfiDriverDiagnostics2ProtocolGuid, DriverDiagnostics2,
                              NULL
                              );
            } else {
              Status = gBS->InstallMultipleProtocolInterfaces (
                              &DriverBinding->DriverBindingHandle,
                              &gEfiDriverBindingProtocolGuid, DriverBinding,
                              &gEfiComponentName2ProtocolGuid, ComponentName2,
                              &gEfiDriverDiagnostics2ProtocolGuid, DriverDiagnostics2,
                              NULL
                              );
            }
          } else {
            if (ComponentName2 == NULL || FeaturePcdGet(PcdComponentName2Disable)) {
              Status = gBS->InstallMultipleProtocolInterfaces (
                              &DriverBinding->DriverBindingHandle,
                              &gEfiDriverBindingProtocolGuid, DriverBinding,
                              &gEfiComponentNameProtocolGuid, ComponentName,
                              &gEfiDriverDiagnostics2ProtocolGuid, DriverDiagnostics2,
                              NULL
                              );
            } else {
              Status = gBS->InstallMultipleProtocolInterfaces (
                              &DriverBinding->DriverBindingHandle,
                              &gEfiDriverBindingProtocolGuid, DriverBinding,
                              &gEfiComponentNameProtocolGuid, ComponentName,
                              &gEfiComponentName2ProtocolGuid, ComponentName2,
                              &gEfiDriverDiagnostics2ProtocolGuid, DriverDiagnostics2,
                              NULL
                              );
            }
          }
        }
      } else {
        if (DriverDiagnostics2 == NULL || FeaturePcdGet(PcdDriverDiagnostics2Disable)) {
          if (ComponentName == NULL || FeaturePcdGet(PcdComponentNameDisable)) {
            if (ComponentName2 == NULL || FeaturePcdGet(PcdComponentName2Disable)) {
              Status = gBS->InstallMultipleProtocolInterfaces (
                              &DriverBinding->DriverBindingHandle,
                              &gEfiDriverBindingProtocolGuid, DriverBinding,
                              &gEfiDriverDiagnosticsProtocolGuid, DriverDiagnostics,
                              NULL
                              );
            } else {
              Status = gBS->InstallMultipleProtocolInterfaces (
                              &DriverBinding->DriverBindingHandle,
                              &gEfiDriverBindingProtocolGuid, DriverBinding,
                              &gEfiComponentName2ProtocolGuid, ComponentName2,
                              &gEfiDriverDiagnosticsProtocolGuid, DriverDiagnostics,
                              NULL
                              );
            }
          } else {
            if (ComponentName2 == NULL || FeaturePcdGet(PcdComponentName2Disable)) {
              Status = gBS->InstallMultipleProtocolInterfaces (
                              &DriverBinding->DriverBindingHandle,
                              &gEfiDriverBindingProtocolGuid, DriverBinding,
                              &gEfiComponentNameProtocolGuid, ComponentName,
                              &gEfiDriverDiagnosticsProtocolGuid, DriverDiagnostics,
                              NULL
                              );
            } else {
              Status = gBS->InstallMultipleProtocolInterfaces (
                              &DriverBinding->DriverBindingHandle,
                              &gEfiDriverBindingProtocolGuid, DriverBinding,
                              &gEfiComponentNameProtocolGuid, ComponentName,
                              &gEfiComponentName2ProtocolGuid, ComponentName2,
                              &gEfiDriverDiagnosticsProtocolGuid, DriverDiagnostics,
                              NULL
                              );
            }
          }
        } else {
          if (ComponentName == NULL || FeaturePcdGet(PcdComponentNameDisable)) {
            if (ComponentName2 == NULL || FeaturePcdGet(PcdComponentName2Disable)) {
              Status = gBS->InstallMultipleProtocolInterfaces (
                              &DriverBinding->DriverBindingHandle,
                              &gEfiDriverBindingProtocolGuid, DriverBinding,
                              &gEfiDriverDiagnosticsProtocolGuid, DriverDiagnostics,
                              &gEfiDriverDiagnostics2ProtocolGuid, DriverDiagnostics2,
                              NULL
                              );
            } else {
              Status = gBS->InstallMultipleProtocolInterfaces (
                              &DriverBinding->DriverBindingHandle,
                              &gEfiDriverBindingProtocolGuid, DriverBinding,
                              &gEfiComponentName2ProtocolGuid, ComponentName2,
                              &gEfiDriverDiagnosticsProtocolGuid, DriverDiagnostics,
                              &gEfiDriverDiagnostics2ProtocolGuid, DriverDiagnostics2,
                              NULL
                              );
            }
          } else {
            if (ComponentName2 == NULL || FeaturePcdGet(PcdComponentName2Disable)) {
              Status = gBS->InstallMultipleProtocolInterfaces (
                              &DriverBinding->DriverBindingHandle,
                              &gEfiDriverBindingProtocolGuid, DriverBinding,
                              &gEfiComponentNameProtocolGuid, ComponentName,
                              &gEfiDriverDiagnosticsProtocolGuid, DriverDiagnostics,
                              &gEfiDriverDiagnostics2ProtocolGuid, DriverDiagnostics2,
                              NULL
                              );
            } else {
              Status = gBS->InstallMultipleProtocolInterfaces (
                              &DriverBinding->DriverBindingHandle,
                              &gEfiDriverBindingProtocolGuid, DriverBinding,
                              &gEfiComponentNameProtocolGuid, ComponentName,
                              &gEfiComponentName2ProtocolGuid, ComponentName2,
                              &gEfiDriverDiagnosticsProtocolGuid, DriverDiagnostics,
                              &gEfiDriverDiagnostics2ProtocolGuid, DriverDiagnostics2,
                              NULL
                              );
            }
          }
        }
      }
    } else {
      if (DriverDiagnostics == NULL || FeaturePcdGet(PcdDriverDiagnosticsDisable)) {
        if (DriverDiagnostics2 == NULL || FeaturePcdGet(PcdDriverDiagnostics2Disable)) {
          if (ComponentName == NULL || FeaturePcdGet(PcdComponentNameDisable)) {
            if (ComponentName2 == NULL || FeaturePcdGet(PcdComponentName2Disable)) {
              Status = gBS->InstallMultipleProtocolInterfaces (
                              &DriverBinding->DriverBindingHandle,
                              &gEfiDriverBindingProtocolGuid, DriverBinding,
                              &gEfiDriverConfigurationProtocolGuid, DriverConfiguration,
                              NULL
                              );
            } else {
              Status = gBS->InstallMultipleProtocolInterfaces (
                              &DriverBinding->DriverBindingHandle,
                              &gEfiDriverBindingProtocolGuid, DriverBinding,
                              &gEfiComponentName2ProtocolGuid, ComponentName2,
                              &gEfiDriverConfigurationProtocolGuid, DriverConfiguration,
                              NULL
                              );
            }
          } else {
            if (ComponentName2 == NULL || FeaturePcdGet(PcdComponentName2Disable)) {
              Status = gBS->InstallMultipleProtocolInterfaces (
                              &DriverBinding->DriverBindingHandle,
                              &gEfiDriverBindingProtocolGuid, DriverBinding,
                              &gEfiComponentNameProtocolGuid, ComponentName,
                              &gEfiDriverConfigurationProtocolGuid, DriverConfiguration,
                              NULL
                              );
            } else {
              Status = gBS->InstallMultipleProtocolInterfaces (
                              &DriverBinding->DriverBindingHandle,
                              &gEfiDriverBindingProtocolGuid, DriverBinding,
                              &gEfiComponentNameProtocolGuid, ComponentName,
                              &gEfiComponentName2ProtocolGuid, ComponentName2,
                              &gEfiDriverConfigurationProtocolGuid, DriverConfiguration,
                              NULL
                              );
            }
          }
        } else {
          if (ComponentName == NULL || FeaturePcdGet(PcdComponentNameDisable)) {
            if (ComponentName2 == NULL || FeaturePcdGet(PcdComponentName2Disable)) {
              Status = gBS->InstallMultipleProtocolInterfaces (
                              &DriverBinding->DriverBindingHandle,
                              &gEfiDriverBindingProtocolGuid, DriverBinding,
                              &gEfiDriverConfigurationProtocolGuid, DriverConfiguration,
                              &gEfiDriverDiagnostics2ProtocolGuid, DriverDiagnostics2,
                              NULL
                              );
            } else {
              Status = gBS->InstallMultipleProtocolInterfaces (
                              &DriverBinding->DriverBindingHandle,
                              &gEfiDriverBindingProtocolGuid, DriverBinding,
                              &gEfiComponentName2ProtocolGuid, ComponentName2,
                              &gEfiDriverConfigurationProtocolGuid, DriverConfiguration,
                              &gEfiDriverDiagnostics2ProtocolGuid, DriverDiagnostics2,
                              NULL
                              );
            }
          } else {
            if (ComponentName2 == NULL || FeaturePcdGet(PcdComponentName2Disable)) {
              Status = gBS->InstallMultipleProtocolInterfaces (
                              &DriverBinding->DriverBindingHandle,
                              &gEfiDriverBindingProtocolGuid, DriverBinding,
                              &gEfiComponentNameProtocolGuid, ComponentName,
                              &gEfiDriverConfigurationProtocolGuid, DriverConfiguration,
                              &gEfiDriverDiagnostics2ProtocolGuid, DriverDiagnostics2,
                              NULL
                              );
            } else {
              Status = gBS->InstallMultipleProtocolInterfaces (
                              &DriverBinding->DriverBindingHandle,
                              &gEfiDriverBindingProtocolGuid, DriverBinding,
                              &gEfiComponentNameProtocolGuid, ComponentName,
                              &gEfiComponentName2ProtocolGuid, ComponentName2,
                              &gEfiDriverConfigurationProtocolGuid, DriverConfiguration,
                              &gEfiDriverDiagnostics2ProtocolGuid, DriverDiagnostics2,
                              NULL
                              );
            }
          }
        }
      } else {
        if (DriverDiagnostics2 == NULL || FeaturePcdGet(PcdDriverDiagnostics2Disable)) {
          if (ComponentName == NULL || FeaturePcdGet(PcdComponentNameDisable)) {
            if (ComponentName2 == NULL || FeaturePcdGet(PcdComponentName2Disable)) {
              Status = gBS->InstallMultipleProtocolInterfaces (
                              &DriverBinding->DriverBindingHandle,
                              &gEfiDriverBindingProtocolGuid, DriverBinding,
                              &gEfiDriverConfigurationProtocolGuid, DriverConfiguration,
                              &gEfiDriverDiagnosticsProtocolGuid, DriverDiagnostics,
                              NULL
                              );
            } else {
              Status = gBS->InstallMultipleProtocolInterfaces (
                              &DriverBinding->DriverBindingHandle,
                              &gEfiDriverBindingProtocolGuid, DriverBinding,
                              &gEfiComponentName2ProtocolGuid, ComponentName2,
                              &gEfiDriverConfigurationProtocolGuid, DriverConfiguration,
                              &gEfiDriverDiagnosticsProtocolGuid, DriverDiagnostics,
                              NULL
                              );
            }
          } else {
            if (ComponentName2 == NULL || FeaturePcdGet(PcdComponentName2Disable)) {
              Status = gBS->InstallMultipleProtocolInterfaces (
                              &DriverBinding->DriverBindingHandle,
                              &gEfiDriverBindingProtocolGuid, DriverBinding,
                              &gEfiComponentNameProtocolGuid, ComponentName,
                              &gEfiDriverConfigurationProtocolGuid, DriverConfiguration,
                              &gEfiDriverDiagnosticsProtocolGuid, DriverDiagnostics,
                              NULL
                              );
            } else {
              Status = gBS->InstallMultipleProtocolInterfaces (
                              &DriverBinding->DriverBindingHandle,
                              &gEfiDriverBindingProtocolGuid, DriverBinding,
                              &gEfiComponentNameProtocolGuid, ComponentName,
                              &gEfiComponentName2ProtocolGuid, ComponentName2,
                              &gEfiDriverConfigurationProtocolGuid, DriverConfiguration,
                              &gEfiDriverDiagnosticsProtocolGuid, DriverDiagnostics,
                              NULL
                              );
            }
          }
        } else {
          if (ComponentName == NULL || FeaturePcdGet(PcdComponentNameDisable)) {
            if (ComponentName2 == NULL || FeaturePcdGet(PcdComponentName2Disable)) {
              Status = gBS->InstallMultipleProtocolInterfaces (
                              &DriverBinding->DriverBindingHandle,
                              &gEfiDriverBindingProtocolGuid, DriverBinding,
                              &gEfiDriverConfigurationProtocolGuid, DriverConfiguration,
                              &gEfiDriverDiagnosticsProtocolGuid, DriverDiagnostics,
                              &gEfiDriverDiagnostics2ProtocolGuid, DriverDiagnostics2,
                              NULL
                              );
            } else {
              Status = gBS->InstallMultipleProtocolInterfaces (
                              &DriverBinding->DriverBindingHandle,
                              &gEfiDriverBindingProtocolGuid, DriverBinding,
                              &gEfiComponentName2ProtocolGuid, ComponentName2,
                              &gEfiDriverConfigurationProtocolGuid, DriverConfiguration,
                              &gEfiDriverDiagnosticsProtocolGuid, DriverDiagnostics,
                              &gEfiDriverDiagnostics2ProtocolGuid, DriverDiagnostics2,
                              NULL
                              );
            }
          } else {
            if (ComponentName2 == NULL || FeaturePcdGet(PcdComponentName2Disable)) {
              Status = gBS->InstallMultipleProtocolInterfaces (
                              &DriverBinding->DriverBindingHandle,
                              &gEfiDriverBindingProtocolGuid, DriverBinding,
                              &gEfiComponentNameProtocolGuid, ComponentName,
                              &gEfiDriverConfigurationProtocolGuid, DriverConfiguration,
                              &gEfiDriverDiagnosticsProtocolGuid, DriverDiagnostics,
                              &gEfiDriverDiagnostics2ProtocolGuid, DriverDiagnostics2,
                              NULL
                              );
            } else {
              Status = gBS->InstallMultipleProtocolInterfaces (
                              &DriverBinding->DriverBindingHandle,
                              &gEfiDriverBindingProtocolGuid, DriverBinding,
                              &gEfiComponentNameProtocolGuid, ComponentName,
                              &gEfiComponentName2ProtocolGuid, ComponentName2,
                              &gEfiDriverConfigurationProtocolGuid, DriverConfiguration,
                              &gEfiDriverDiagnosticsProtocolGuid, DriverDiagnostics,
                              &gEfiDriverDiagnostics2ProtocolGuid, DriverDiagnostics2,
                              NULL
                              );
            }
          }
        }
      }
    }
  } else {
    if (DriverConfiguration == NULL) {
      if (DriverDiagnostics == NULL || FeaturePcdGet(PcdDriverDiagnosticsDisable)) {
        if (DriverDiagnostics2 == NULL || FeaturePcdGet(PcdDriverDiagnostics2Disable)) {
          if (ComponentName == NULL || FeaturePcdGet(PcdComponentNameDisable)) {
            if (ComponentName2 == NULL || FeaturePcdGet(PcdComponentName2Disable)) {
              Status = gBS->InstallMultipleProtocolInterfaces (
                              &DriverBinding->DriverBindingHandle,
                              &gEfiDriverBindingProtocolGuid, DriverBinding,
                              &gEfiDriverConfiguration2ProtocolGuid, DriverConfiguration2,
                              NULL
                              );
            } else {
              Status = gBS->InstallMultipleProtocolInterfaces (
                              &DriverBinding->DriverBindingHandle,
                              &gEfiDriverBindingProtocolGuid, DriverBinding,
                              &gEfiComponentName2ProtocolGuid, ComponentName2,
                              &gEfiDriverConfiguration2ProtocolGuid, DriverConfiguration2,
                              NULL
                              );
            }
          } else {
            if (ComponentName2 == NULL || FeaturePcdGet(PcdComponentName2Disable)) {
              Status = gBS->InstallMultipleProtocolInterfaces (
                              &DriverBinding->DriverBindingHandle,
                              &gEfiDriverBindingProtocolGuid, DriverBinding,
                              &gEfiComponentNameProtocolGuid, ComponentName,
                              &gEfiDriverConfiguration2ProtocolGuid, DriverConfiguration2,
                              NULL
                              );
            } else {
              Status = gBS->InstallMultipleProtocolInterfaces (
                              &DriverBinding->DriverBindingHandle,
                              &gEfiDriverBindingProtocolGuid, DriverBinding,
                              &gEfiComponentNameProtocolGuid, ComponentName,
                              &gEfiComponentName2ProtocolGuid, ComponentName2,
                              &gEfiDriverConfiguration2ProtocolGuid, DriverConfiguration2,
                              NULL
                              );
            }
          }
        } else {
          if (ComponentName == NULL || FeaturePcdGet(PcdComponentNameDisable)) {
            if (ComponentName2 == NULL || FeaturePcdGet(PcdComponentName2Disable)) {
              Status = gBS->InstallMultipleProtocolInterfaces (
                              &DriverBinding->DriverBindingHandle,
                              &gEfiDriverBindingProtocolGuid, DriverBinding,
                              &gEfiDriverDiagnostics2ProtocolGuid, DriverDiagnostics2,
                              &gEfiDriverConfiguration2ProtocolGuid, DriverConfiguration2,
                              NULL
                              );
            } else {
              Status = gBS->InstallMultipleProtocolInterfaces (
                              &DriverBinding->DriverBindingHandle,
                              &gEfiDriverBindingProtocolGuid, DriverBinding,
                              &gEfiComponentName2ProtocolGuid, ComponentName2,
                              &gEfiDriverConfiguration2ProtocolGuid, DriverConfiguration2,
                              &gEfiDriverDiagnostics2ProtocolGuid, DriverDiagnostics2,
                              NULL
                              );
            }
          } else {
            if (ComponentName2 == NULL || FeaturePcdGet(PcdComponentName2Disable)) {
              Status = gBS->InstallMultipleProtocolInterfaces (
                              &DriverBinding->DriverBindingHandle,
                              &gEfiDriverBindingProtocolGuid, DriverBinding,
                              &gEfiComponentNameProtocolGuid, ComponentName,
                              &gEfiDriverConfiguration2ProtocolGuid, DriverConfiguration2,
                              &gEfiDriverDiagnostics2ProtocolGuid, DriverDiagnostics2,
                              NULL
                              );
            } else {
              Status = gBS->InstallMultipleProtocolInterfaces (
                              &DriverBinding->DriverBindingHandle,
                              &gEfiDriverBindingProtocolGuid, DriverBinding,
                              &gEfiComponentNameProtocolGuid, ComponentName,
                              &gEfiComponentName2ProtocolGuid, ComponentName2,
                              &gEfiDriverConfiguration2ProtocolGuid, DriverConfiguration2,
                              &gEfiDriverDiagnostics2ProtocolGuid, DriverDiagnostics2,
                              NULL
                              );
            }
          }
        }
      } else {
        if (DriverDiagnostics2 == NULL || FeaturePcdGet(PcdDriverDiagnostics2Disable)) {
          if (ComponentName == NULL || FeaturePcdGet(PcdComponentNameDisable)) {
            if (ComponentName2 == NULL || FeaturePcdGet(PcdComponentName2Disable)) {
              Status = gBS->InstallMultipleProtocolInterfaces (
                              &DriverBinding->DriverBindingHandle,
                              &gEfiDriverBindingProtocolGuid, DriverBinding,
                              &gEfiDriverDiagnosticsProtocolGuid, DriverDiagnostics,
                              &gEfiDriverConfiguration2ProtocolGuid, DriverConfiguration2,
                              NULL
                              );
            } else {
              Status = gBS->InstallMultipleProtocolInterfaces (
                              &DriverBinding->DriverBindingHandle,
                              &gEfiDriverBindingProtocolGuid, DriverBinding,
                              &gEfiComponentName2ProtocolGuid, ComponentName2,
                              &gEfiDriverConfiguration2ProtocolGuid, DriverConfiguration2,
                              &gEfiDriverDiagnosticsProtocolGuid, DriverDiagnostics,
                              NULL
                              );
            }
          } else {
            if (ComponentName2 == NULL || FeaturePcdGet(PcdComponentName2Disable)) {
              Status = gBS->InstallMultipleProtocolInterfaces (
                              &DriverBinding->DriverBindingHandle,
                              &gEfiDriverBindingProtocolGuid, DriverBinding,
                              &gEfiComponentNameProtocolGuid, ComponentName,
                              &gEfiDriverDiagnosticsProtocolGuid, DriverDiagnostics,
                              &gEfiDriverConfiguration2ProtocolGuid, DriverConfiguration2,
                              NULL
                              );
            } else {
              Status = gBS->InstallMultipleProtocolInterfaces (
                              &DriverBinding->DriverBindingHandle,
                              &gEfiDriverBindingProtocolGuid, DriverBinding,
                              &gEfiComponentNameProtocolGuid, ComponentName,
                              &gEfiComponentName2ProtocolGuid, ComponentName2,
                              &gEfiDriverConfiguration2ProtocolGuid, DriverConfiguration2,
                              &gEfiDriverDiagnosticsProtocolGuid, DriverDiagnostics,
                              NULL
                              );
            }
          }
        } else {
          if (ComponentName == NULL || FeaturePcdGet(PcdComponentNameDisable)) {
            if (ComponentName2 == NULL || FeaturePcdGet(PcdComponentName2Disable)) {
              Status = gBS->InstallMultipleProtocolInterfaces (
                              &DriverBinding->DriverBindingHandle,
                              &gEfiDriverBindingProtocolGuid, DriverBinding,
                              &gEfiDriverConfiguration2ProtocolGuid, DriverConfiguration2,
                              &gEfiDriverDiagnosticsProtocolGuid, DriverDiagnostics,
                              &gEfiDriverDiagnostics2ProtocolGuid, DriverDiagnostics2,
                              NULL
                              );
            } else {
              Status = gBS->InstallMultipleProtocolInterfaces (
                              &DriverBinding->DriverBindingHandle,
                              &gEfiDriverBindingProtocolGuid, DriverBinding,
                              &gEfiComponentName2ProtocolGuid, ComponentName2,
                              &gEfiDriverConfiguration2ProtocolGuid, DriverConfiguration2,
                              &gEfiDriverDiagnosticsProtocolGuid, DriverDiagnostics,
                              &gEfiDriverDiagnostics2ProtocolGuid, DriverDiagnostics2,
                              NULL
                              );
            }
          } else {
            if (ComponentName2 == NULL || FeaturePcdGet(PcdComponentName2Disable)) {
              Status = gBS->InstallMultipleProtocolInterfaces (
                              &DriverBinding->DriverBindingHandle,
                              &gEfiDriverBindingProtocolGuid, DriverBinding,
                              &gEfiComponentNameProtocolGuid, ComponentName,
                              &gEfiDriverConfiguration2ProtocolGuid, DriverConfiguration2,
                              &gEfiDriverDiagnosticsProtocolGuid, DriverDiagnostics,
                              &gEfiDriverDiagnostics2ProtocolGuid, DriverDiagnostics2,
                              NULL
                              );
            } else {
              Status = gBS->InstallMultipleProtocolInterfaces (
                              &DriverBinding->DriverBindingHandle,
                              &gEfiDriverBindingProtocolGuid, DriverBinding,
                              &gEfiComponentNameProtocolGuid, ComponentName,
                              &gEfiComponentName2ProtocolGuid, ComponentName2,
                              &gEfiDriverConfiguration2ProtocolGuid, DriverConfiguration2,
                              &gEfiDriverDiagnosticsProtocolGuid, DriverDiagnostics,
                              &gEfiDriverDiagnostics2ProtocolGuid, DriverDiagnostics2,
                              NULL
                              );
            }
          }
        }
      }
    } else {
      if (DriverDiagnostics == NULL || FeaturePcdGet(PcdDriverDiagnosticsDisable)) {
        if (DriverDiagnostics2 == NULL || FeaturePcdGet(PcdDriverDiagnostics2Disable)) {
          if (ComponentName == NULL || FeaturePcdGet(PcdComponentNameDisable)) {
            if (ComponentName2 == NULL || FeaturePcdGet(PcdComponentName2Disable)) {
              Status = gBS->InstallMultipleProtocolInterfaces (
                              &DriverBinding->DriverBindingHandle,
                              &gEfiDriverBindingProtocolGuid, DriverBinding,
                              &gEfiDriverConfigurationProtocolGuid, DriverConfiguration,
                              &gEfiDriverConfiguration2ProtocolGuid, DriverConfiguration2,
                              NULL
                              );
            } else {
              Status = gBS->InstallMultipleProtocolInterfaces (
                              &DriverBinding->DriverBindingHandle,
                              &gEfiDriverBindingProtocolGuid, DriverBinding,
                              &gEfiComponentName2ProtocolGuid, ComponentName2,
                              &gEfiDriverConfigurationProtocolGuid, DriverConfiguration,
                              &gEfiDriverConfiguration2ProtocolGuid, DriverConfiguration2,
                              NULL
                              );
            }
          } else {
            if (ComponentName2 == NULL || FeaturePcdGet(PcdComponentName2Disable)) {
              Status = gBS->InstallMultipleProtocolInterfaces (
                              &DriverBinding->DriverBindingHandle,
                              &gEfiDriverBindingProtocolGuid, DriverBinding,
                              &gEfiComponentNameProtocolGuid, ComponentName,
                              &gEfiDriverConfigurationProtocolGuid, DriverConfiguration,
                              &gEfiDriverConfiguration2ProtocolGuid, DriverConfiguration2,
                              NULL
                              );
            } else {
              Status = gBS->InstallMultipleProtocolInterfaces (
                              &DriverBinding->DriverBindingHandle,
                              &gEfiDriverBindingProtocolGuid, DriverBinding,
                              &gEfiComponentNameProtocolGuid, ComponentName,
                              &gEfiComponentName2ProtocolGuid, ComponentName2,
                              &gEfiDriverConfigurationProtocolGuid, DriverConfiguration,
                              &gEfiDriverConfiguration2ProtocolGuid, DriverConfiguration2,
                              NULL
                              );
            }
          }
        } else {
          if (ComponentName == NULL || FeaturePcdGet(PcdComponentNameDisable)) {
            if (ComponentName2 == NULL || FeaturePcdGet(PcdComponentName2Disable)) {
              Status = gBS->InstallMultipleProtocolInterfaces (
                              &DriverBinding->DriverBindingHandle,
                              &gEfiDriverBindingProtocolGuid, DriverBinding,
                              &gEfiDriverConfigurationProtocolGuid, DriverConfiguration,
                              &gEfiDriverConfiguration2ProtocolGuid, DriverConfiguration2,
                              &gEfiDriverDiagnostics2ProtocolGuid, DriverDiagnostics2,
                              NULL
                              );
            } else {
              Status = gBS->InstallMultipleProtocolInterfaces (
                              &DriverBinding->DriverBindingHandle,
                              &gEfiDriverBindingProtocolGuid, DriverBinding,
                              &gEfiComponentName2ProtocolGuid, ComponentName2,
                              &gEfiDriverConfigurationProtocolGuid, DriverConfiguration,
                              &gEfiDriverConfiguration2ProtocolGuid, DriverConfiguration2,
                              &gEfiDriverDiagnostics2ProtocolGuid, DriverDiagnostics2,
                              NULL
                              );
            }
          } else {
            if (ComponentName2 == NULL || FeaturePcdGet(PcdComponentName2Disable)) {
              Status = gBS->InstallMultipleProtocolInterfaces (
                              &DriverBinding->DriverBindingHandle,
                              &gEfiDriverBindingProtocolGuid, DriverBinding,
                              &gEfiComponentNameProtocolGuid, ComponentName,
                              &gEfiDriverConfigurationProtocolGuid, DriverConfiguration,
                              &gEfiDriverConfiguration2ProtocolGuid, DriverConfiguration2,
                              &gEfiDriverDiagnostics2ProtocolGuid, DriverDiagnostics2,
                              NULL
                              );
            } else {
              Status = gBS->InstallMultipleProtocolInterfaces (
                              &DriverBinding->DriverBindingHandle,
                              &gEfiDriverBindingProtocolGuid, DriverBinding,
                              &gEfiComponentNameProtocolGuid, ComponentName,
                              &gEfiComponentName2ProtocolGuid, ComponentName2,
                              &gEfiDriverConfigurationProtocolGuid, DriverConfiguration,
                              &gEfiDriverConfiguration2ProtocolGuid, DriverConfiguration2,
                              &gEfiDriverDiagnostics2ProtocolGuid, DriverDiagnostics2,
                              NULL
                              );
            }
          }
        }
      } else {
        if (DriverDiagnostics2 == NULL || FeaturePcdGet(PcdDriverDiagnostics2Disable)) {
          if (ComponentName == NULL || FeaturePcdGet(PcdComponentNameDisable)) {
            if (ComponentName2 == NULL || FeaturePcdGet(PcdComponentName2Disable)) {
              Status = gBS->InstallMultipleProtocolInterfaces (
                              &DriverBinding->DriverBindingHandle,
                              &gEfiDriverBindingProtocolGuid, DriverBinding,
                              &gEfiDriverConfigurationProtocolGuid, DriverConfiguration,
                              &gEfiDriverConfiguration2ProtocolGuid, DriverConfiguration2,
                              &gEfiDriverDiagnosticsProtocolGuid, DriverDiagnostics,
                              NULL
                              );
            } else {
              Status = gBS->InstallMultipleProtocolInterfaces (
                              &DriverBinding->DriverBindingHandle,
                              &gEfiDriverBindingProtocolGuid, DriverBinding,
                              &gEfiComponentName2ProtocolGuid, ComponentName2,
                              &gEfiDriverConfigurationProtocolGuid, DriverConfiguration,
                              &gEfiDriverConfiguration2ProtocolGuid, DriverConfiguration2,
                              &gEfiDriverDiagnosticsProtocolGuid, DriverDiagnostics,
                              NULL
                              );
            }
          } else {
            if (ComponentName2 == NULL || FeaturePcdGet(PcdComponentName2Disable)) {
              Status = gBS->InstallMultipleProtocolInterfaces (
                              &DriverBinding->DriverBindingHandle,
                              &gEfiDriverBindingProtocolGuid, DriverBinding,
                              &gEfiComponentNameProtocolGuid, ComponentName,
                              &gEfiDriverConfigurationProtocolGuid, DriverConfiguration,
                              &gEfiDriverConfiguration2ProtocolGuid, DriverConfiguration2,
                              &gEfiDriverDiagnosticsProtocolGuid, DriverDiagnostics,
                              NULL
                              );
            } else {
              Status = gBS->InstallMultipleProtocolInterfaces (
                              &DriverBinding->DriverBindingHandle,
                              &gEfiDriverBindingProtocolGuid, DriverBinding,
                              &gEfiComponentNameProtocolGuid, ComponentName,
                              &gEfiComponentName2ProtocolGuid, ComponentName2,
                              &gEfiDriverConfigurationProtocolGuid, DriverConfiguration,
                              &gEfiDriverConfiguration2ProtocolGuid, DriverConfiguration2,
                              &gEfiDriverDiagnosticsProtocolGuid, DriverDiagnostics,
                              NULL
                              );
            }
          }
        } else {
          if (ComponentName == NULL || FeaturePcdGet(PcdComponentNameDisable)) {
            if (ComponentName2 == NULL || FeaturePcdGet(PcdComponentName2Disable)) {
              Status = gBS->InstallMultipleProtocolInterfaces (
                              &DriverBinding->DriverBindingHandle,
                              &gEfiDriverBindingProtocolGuid, DriverBinding,
                              &gEfiDriverConfigurationProtocolGuid, DriverConfiguration,
                              &gEfiDriverConfiguration2ProtocolGuid, DriverConfiguration2,
                              &gEfiDriverDiagnosticsProtocolGuid, DriverDiagnostics,
                              &gEfiDriverDiagnostics2ProtocolGuid, DriverDiagnostics2,
                              NULL
                              );
            } else {
              Status = gBS->InstallMultipleProtocolInterfaces (
                              &DriverBinding->DriverBindingHandle,
                              &gEfiDriverBindingProtocolGuid, DriverBinding,
                              &gEfiComponentName2ProtocolGuid, ComponentName2,
                              &gEfiDriverConfigurationProtocolGuid, DriverConfiguration,
                              &gEfiDriverConfiguration2ProtocolGuid, DriverConfiguration2,
                              &gEfiDriverDiagnosticsProtocolGuid, DriverDiagnostics,
                              &gEfiDriverDiagnostics2ProtocolGuid, DriverDiagnostics2,
                              NULL
                              );
            }
          } else {
            if (ComponentName2 == NULL || FeaturePcdGet(PcdComponentName2Disable)) {
              Status = gBS->InstallMultipleProtocolInterfaces (
                              &DriverBinding->DriverBindingHandle,
                              &gEfiDriverBindingProtocolGuid, DriverBinding,
                              &gEfiComponentNameProtocolGuid, ComponentName,
                              &gEfiDriverConfigurationProtocolGuid, DriverConfiguration,
                              &gEfiDriverConfiguration2ProtocolGuid, DriverConfiguration2,
                              &gEfiDriverDiagnosticsProtocolGuid, DriverDiagnostics,
                              &gEfiDriverDiagnostics2ProtocolGuid, DriverDiagnostics2,
                              NULL
                              );
            } else {
              Status = gBS->InstallMultipleProtocolInterfaces (
                              &DriverBinding->DriverBindingHandle,
                              &gEfiDriverBindingProtocolGuid, DriverBinding,
                              &gEfiComponentNameProtocolGuid, ComponentName,
                              &gEfiComponentName2ProtocolGuid, ComponentName2,
                              &gEfiDriverConfigurationProtocolGuid, DriverConfiguration,
                              &gEfiDriverConfiguration2ProtocolGuid, DriverConfiguration2,
                              &gEfiDriverDiagnosticsProtocolGuid, DriverDiagnostics,
                              &gEfiDriverDiagnostics2ProtocolGuid, DriverDiagnostics2,
                              NULL
                              );
            }
          }
        }
      }
    }
  }

  //
  // ASSERT if the call to InstallMultipleProtocolInterfaces() failed
  //
  ASSERT_EFI_ERROR (Status);

  return Status;
}
