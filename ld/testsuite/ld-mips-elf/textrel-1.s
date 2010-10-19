        .globl foo
foo:
        .cfi_startproc
        nop
        .cfi_def_cfa_offset 4
        nop
        .cfi_register $29, $0
        nop
        .cfi_endproc
