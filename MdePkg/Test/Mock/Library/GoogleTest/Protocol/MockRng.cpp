/** @file MockRng.cpp
  Google Test mock for Rng Protocol

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <GoogleTest/Protocol/MockRng.h>

MOCK_INTERFACE_DEFINITION (MockRng);
MOCK_FUNCTION_DEFINITION (MockRng, GetInfo, 3, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockRng, GetRng, 4, EFIAPI);

EFI_RNG_PROTOCOL  RNG_PROTOCOL_INSTANCE = {
  GetInfo, // EFI_RNG_GET_INFO
  GetRng   // EFI_RNG_GET_RNG
};

extern "C" {
  EFI_RNG_PROTOCOL  *gRngProtocol = &RNG_PROTOCOL_INSTANCE;
}
