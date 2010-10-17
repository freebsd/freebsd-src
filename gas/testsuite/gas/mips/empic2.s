# Check assembly of and relocs for -membedded-pic la, lw, ld, sw, sd macros.

        .text
        .set noreorder

start:
	nop

	.globl	g1
	.ent	g1
i1:				# 0x00004
g1:
	.space 0x8000
	nop
	.end	g1

	.globl	g2
	.ent	g2
i2:				# 0x08008
g2:
	.space 0x8000
	nop
	.end	g2

	.globl	g3
	.ent	g3
i3:				# 0x1000c
g3:

	la	$2, (i1 - i3)($4)
	la	$2, (g1 - i3)($4)
	la	$2, (i2 - i3)($4)
	la	$2, (g2 - i3)($4)
	la	$2, (if - i3)($4)
	la	$2, (gf - i3)($4)
	la	$2, (e  - i3)($4)
	la	$2, (i1 - g3)($4)
	la	$2, (g1 - g3)($4)
	la	$2, (i2 - g3)($4)
	la	$2, (g2 - g3)($4)
	la	$2, (if - g3)($4)
	la	$2, (gf - g3)($4)
	la	$2, (e  - g3)($4)

	la	$2, (i1 - i3)
	la	$2, (g1 - i3)
	la	$2, (i2 - i3)
	la	$2, (g2 - i3)
	la	$2, (if - i3)
	la	$2, (gf - i3)
	la	$2, (e  - i3)
	la	$2, (i1 - g3)
	la	$2, (g1 - g3)
	la	$2, (i2 - g3)
	la	$2, (g2 - g3)
	la	$2, (if - g3)
	la	$2, (gf - g3)
	la	$2, (e  - g3)

	lw	$2, (i1 - i3)($4)
	lw	$2, (g1 - i3)($4)
	lw	$2, (i2 - i3)($4)
	lw	$2, (g2 - i3)($4)
	lw	$2, (if - i3)($4)
	lw	$2, (gf - i3)($4)
	lw	$2, (e  - i3)($4)
	ld	$2, (i1 - g3)($4)
	ld	$2, (g1 - g3)($4)
	ld	$2, (i2 - g3)($4)
	ld	$2, (g2 - g3)($4)
	ld	$2, (if - g3)($4)
	ld	$2, (gf - g3)($4)
	ld	$2, (e  - g3)($4)

	sw	$2, (i1 - i3)($4)
	sw	$2, (g1 - i3)($4)
	sw	$2, (i2 - i3)($4)
	sw	$2, (g2 - i3)($4)
	sw	$2, (if - i3)($4)
	sw	$2, (gf - i3)($4)
	sw	$2, (e  - i3)($4)
	sd	$2, (i1 - g3)($4)
	sd	$2, (g1 - g3)($4)
	sd	$2, (i2 - g3)($4)
	sd	$2, (g2 - g3)($4)
	sd	$2, (if - g3)($4)
	sd	$2, (gf - g3)($4)
	sd	$2, (e  - g3)($4)

	.end	g3

	.globl	gf
	.ent	gf
if:
gf:
	nop
	.end	gf

# Force at least 8 (non-delay-slot) zero bytes, to make 'objdump' print ...
      .space  8
