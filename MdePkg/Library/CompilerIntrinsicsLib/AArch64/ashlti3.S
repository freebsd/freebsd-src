#------------------------------------------------------------------------------
#
# Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
#------------------------------------------------------------------------------

#include <AsmMacroLib.h>

ASM_FUNC(__ashlti3)
  # return if shift is 0
  cbz x2, 1f

  mov x3, #64
  sub x3, x3, x2
  cmp x3, #0
  b.le 2f

  # shift is <= 64 bits
  lsr x3, x0, x3
  lsl x1, x1, x2
  orr x1, x1, x3
  lsl x0, x0, x2
1:
  ret

2:
  # shift is > 64
  neg w3, w3
  lsl x1, x0, x3
  mov x0, #0
  ret
