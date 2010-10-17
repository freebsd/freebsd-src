# MIPS ELF reloc 5

        .data
        .align  2
sp1:
        .space  60
        .globl  dg1
dg1:
dl1:
        .space  60


        .text

        .ent    fn
        .type   fn,@function
fn:
        la      $5,dg1+0
        la      $5,dg1+12
        la      $5,dg1+0($17)
        la      $5,dg1+12($17)
        lw      $5,dg1+0
        lw      $5,dg1+12
        lw      $5,dg1+0($17)
        lw      $5,dg1+12($17)

        la      $5,dl1+0
        la      $5,dl1+12
        la      $5,dl1+0($17)
        la      $5,dl1+12($17)
        lw      $5,dl1+0
        lw      $5,dl1+12
        lw      $5,dl1+0($17)
        lw      $5,dl1+12($17)

        la      $5,dg2+0
        la      $5,dg2+12
        la      $5,dg2+0($17)
        la      $5,dg2+12($17)
        lw      $5,dg2+0
        lw      $5,dg2+12
        lw      $5,dg2+0($17)
        lw      $5,dg2+12($17)

        la      $5,dl2+0
        la      $5,dl2+12
        la      $5,dl2+0($17)
        la      $5,dl2+12($17)
        lw      $5,dl2+0
        lw      $5,dl2+12
        lw      $5,dl2+0($17)
        lw      $5,dl2+12($17)

# Force at least 8 (non-delay-slot) zero bytes, to make 'objdump' print ...
	.space	8

        .end    fn

        .data
        .align  2
sp2:
        .space  60
        .globl  dg2
dg2:
dl2:
        .space  60

