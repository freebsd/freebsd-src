	.text

# some data
	.word	0xdeadbeef
	.word	0xdeadbeef
	.word	0xdeadbeef
	.word	0xdeadbeef

# some real code, compiled from a toy C program
	        .globl  main
        .ent    main
main:
        .frame  $fp,32,$31              # vars= 16, regs= 2/0, args= 0, extra= 0
        .mask   0xc0000000,-8
        .fmask  0x00000000,0
        subu    $sp,$sp,32
        sd      $31,24($sp)
        sd      $fp,16($sp)
        move    $fp,$sp
        jal     __main
        li      $2,2                    # 0x2
        sw      $2,0($fp)
        lw      $2,0($fp)
        move    $3,$2
        sll     $4,$3,1
        addu    $2,$4,$2
        sw      $2,4($fp)
        lw      $4,4($fp)
        jal     g
        lw      $3,0($fp)
        move    $2,$3
        b       $L1
$L1:
        move    $sp,$fp
        ld      $31,24($sp)
        ld      $fp,16($sp)
        addu    $sp,$sp,32
        j       $31
        .end    main
        .align  2
        .globl  g
        .ent    g
g:
        .frame  $fp,32,$31              # vars= 16, regs= 1/0, args= 0, extra= 0
        .mask   0x40000000,-16
        .fmask  0x00000000,0
        subu    $sp,$sp,32
        sd      $fp,16($sp)
        move    $fp,$sp
        sw      $4,0($fp)
        lw      $2,0($fp)
        addu    $3,$2,1
        move    $2,$3
        b       $L2
$L2:
        move    $sp,$fp
        ld      $fp,16($sp)
        addu    $sp,$sp,32
        j       $31
        .end    g
