# Check PC-relative HI/LO relocs relocs for -membedded-pic when HI and
# LO are split over a 32K boundary.

        .text
        .set noreorder

	SYM_TO_TEST = g1

	.globl	ext

	.org	0x00000
	.globl	g1
g1:
l1:

	.org	0x08000
	.globl	fn
	.ent	fn
fn:
	.org	(0x10000 - 4)
	la	$2, SYM_TO_TEST - fn		# expands to 2 instructions

	.org	(0x18000 - 4)
	la	$2, SYM_TO_TEST - fn		# expands to 2 instructions

	.org	(0x20000 - 4)
	la	$2, (SYM_TO_TEST - fn)($3)	# expands to 3 instructions

	.org	(0x28000 - 4)
	la	$2, (SYM_TO_TEST - fn)($3)	# expands to 3 instructions

	.org	(0x30000 - 8)
	la	$2, (SYM_TO_TEST - fn)($3)	# expands to 3 instructions

	.org	(0x38000 - 8)
	la	$2, (SYM_TO_TEST - fn)($3)	# expands to 3 instructions

	.end fn

	.org	0x40000
	.globl	g2
g2:
l2:

# Force at least 8 (non-delay-slot) zero bytes, to make 'objdump' print ...
      .space  8
