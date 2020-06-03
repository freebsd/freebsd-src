;------------------------------------------------------------------------------
;
; Copyright (c) 2019, Intel Corporation. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
; Abstract:
;
;   This file provides macro definitions for NASM files.
;
;------------------------------------------------------------------------------

%macro SETSSBSY        0
    DB 0xF3, 0x0F, 0x01, 0xE8
%endmacro

%macro READSSP_RAX     0
    DB 0xF3, 0x48, 0x0F, 0x1E, 0xC8
%endmacro

%macro INCSSP_RAX      0
    DB 0xF3, 0x48, 0x0F, 0xAE, 0xE8
%endmacro
