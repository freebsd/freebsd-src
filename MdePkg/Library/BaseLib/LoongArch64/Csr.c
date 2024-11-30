/** @file
  LoongArch CSR operation functions.

  Copyright (c) 2024, Loongson Technology Corporation Limited. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

UINTN
AsmCsrRead (
  IN UINT16  Select
  );

UINTN
AsmCsrWrite (
  IN UINT16  Select,
  IN UINTN   Value
  );

UINTN
AsmCsrXChg (
  IN UINT16  Select,
  IN UINTN   Value,
  IN UINTN   Mask
  );

/**
  CSR read operation.

  @param[in]  Select   CSR read instruction select values.

  @return     The return value of csrrd instruction,
              if a break exception is triggered, the Select is out of support.
**/
UINTN
EFIAPI
CsrRead (
  IN UINT16  Select
  )
{
  return AsmCsrRead (Select);
}

/**
  CSR write operation.

  @param[in]       Select  CSR write instruction select values.
  @param[in, out]  Value   The csrwr will write the value.

  @return     The return value of csrwr instruction, that is, store the old value of
              the register, if a break exception is triggered, the Select is out of support.
**/
UINTN
EFIAPI
CsrWrite (
  IN     UINT16  Select,
  IN OUT UINTN   Value
  )
{
  return AsmCsrWrite (Select, Value);
}

/**
  CSR exchange operation.

  @param[in]       Select   CSR exchange instruction select values.
  @param[in, out]  Value    The csrxchg will write the value.
  @param[in]       Mask     The csrxchg mask value.

  @return     The return value of csrxchg instruction, that is, store the old value of
              the register, if a break exception is triggered, the Select is out of support.
**/
UINTN
EFIAPI
CsrXChg (
  IN     UINT16  Select,
  IN OUT UINTN   Value,
  IN     UINTN   Mask
  )
{
  return AsmCsrXChg (Select, Value, Mask);
}
