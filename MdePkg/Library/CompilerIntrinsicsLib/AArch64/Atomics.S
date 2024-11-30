#------------------------------------------------------------------------------
#
# Copyright (c) 2020, Arm, Limited. All rights reserved.<BR>
#
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
#------------------------------------------------------------------------------

        /*
         * Provide the GCC intrinsics that are required when using GCC 9 or
         * later with the -moutline-atomics options (which became the default
         * in GCC 10)
         */
        .arch armv8-a

        .macro          reg_alias, pfx, sz
        r0_\sz          .req    \pfx\()0
        r1_\sz          .req    \pfx\()1
        tmp0_\sz        .req    \pfx\()16
        tmp1_\sz        .req    \pfx\()17
        .endm

        /*
         * Define register aliases of the right type for each size
         * (xN for 8 bytes, wN for everything smaller)
         */
        reg_alias       w, 1
        reg_alias       w, 2
        reg_alias       w, 4
        reg_alias       x, 8

        .macro          fn_start, name:req
        .section        .text.\name
        .globl          \name
        .type           \name, %function
\name\():
        .endm

        .macro          fn_end, name:req
        .size           \name, . - \name
        .endm

        /*
         * Emit an atomic helper for \model with operands of size \sz, using
         * the operation specified by \insn (which is the LSE name), and which
         * can be implemented using the generic load-locked/store-conditional
         * (LL/SC) sequence below, using the arithmetic operation given by
         * \opc.
         */
        .macro          emit_ld_sz, sz:req, insn:req, opc:req, model:req, s, a, l
        fn_start        __aarch64_\insn\()\sz\()\model
        mov             tmp0_\sz, r0_\sz
0:      ld\a\()xr\s     r0_\sz, [x1]
        .ifnc           \insn, swp
        \opc            tmp1_\sz, r0_\sz, tmp0_\sz
        st\l\()xr\s     w15, tmp1_\sz, [x1]
        .else
        st\l\()xr\s     w15, tmp0_\sz, [x1]
        .endif
        cbnz            w15, 0b
        ret
        fn_end          __aarch64_\insn\()\sz\()\model
        .endm

        /*
         * Emit atomic helpers for \model for operand sizes in the
         * set {1, 2, 4, 8}, for the instruction pattern given by
         * \insn. (This is the LSE name, but this implementation uses
         * the generic LL/SC sequence using \opc as the arithmetic
         * operation on the target.)
         */
        .macro          emit_ld, insn:req, opc:req, model:req, a, l
        emit_ld_sz      1, \insn, \opc, \model, b, \a, \l
        emit_ld_sz      2, \insn, \opc, \model, h, \a, \l
        emit_ld_sz      4, \insn, \opc, \model,  , \a, \l
        emit_ld_sz      8, \insn, \opc, \model,  , \a, \l
        .endm

        /*
         * Emit the compare and swap helper for \model and size \sz
         * using LL/SC instructions.
         */
        .macro          emit_cas_sz, sz:req, model:req, uxt:req, s, a, l
        fn_start        __aarch64_cas\sz\()\model
        \uxt            tmp0_\sz, r0_\sz
0:      ld\a\()xr\s     r0_\sz, [x2]
        cmp             r0_\sz, tmp0_\sz
        bne             1f
        st\l\()xr\s     w15, r1_\sz, [x2]
        cbnz            w15, 0b
1:      ret
        fn_end          __aarch64_cas\sz\()\model
        .endm

        /*
         * Emit compare-and-swap helpers for \model for operand sizes in the
         * set {1, 2, 4, 8, 16}.
         */
        .macro          emit_cas, model:req, a, l
        emit_cas_sz     1, \model, uxtb, b, \a, \l
        emit_cas_sz     2, \model, uxth, h, \a, \l
        emit_cas_sz     4, \model, mov ,  , \a, \l
        emit_cas_sz     8, \model, mov ,  , \a, \l

        /*
         * We cannot use the parameterized sequence for 16 byte CAS, so we
         * need to define it explicitly.
         */
        fn_start        __aarch64_cas16\model
        mov             x16, x0
        mov             x17, x1
0:      ld\a\()xp       x0, x1, [x4]
        cmp             x0, x16
        ccmp            x1, x17, #0, eq
        bne             1f
        st\l\()xp       w15, x16, x17, [x4]
        cbnz            w15, 0b
1:      ret
        fn_end          __aarch64_cas16\model
        .endm

        /*
         * Emit the set of GCC outline atomic helper functions for
         * the memory ordering model given by \model:
         * - relax      unordered loads and stores
         * - acq        load-acquire, unordered store
         * - rel        unordered load, store-release
         * - acq_rel    load-acquire, store-release
         */
        .macro          emit_model, model:req, a, l
        emit_ld         ldadd, add, \model, \a, \l
        emit_ld         ldclr, bic, \model, \a, \l
        emit_ld         ldeor, eor, \model, \a, \l
        emit_ld         ldset, orr, \model, \a, \l
        emit_ld         swp,   mov, \model, \a, \l
        emit_cas        \model, \a, \l
        .endm

        emit_model      _relax
        emit_model      _acq, a
        emit_model      _rel,, l
        emit_model      _acq_rel, a, l
