# source file to test objdump's disassembly using various styles of
# GPR names.

	.set noreorder
	.set noat

	.globl text_label .text
text_label:

	lui	$0, 0
	lui	$1, 0
	lui	$2, 0
	lui	$3, 0
	lui	$4, 0
	lui	$5, 0
	lui	$6, 0
	lui	$7, 0
	lui	$8, 0
	lui	$9, 0
	lui	$10, 0
	lui	$11, 0
	lui	$12, 0
	lui	$13, 0
	lui	$14, 0
	lui	$15, 0
	lui	$16, 0
	lui	$17, 0
	lui	$18, 0
	lui	$19, 0
	lui	$20, 0
	lui	$21, 0
	lui	$22, 0
	lui	$23, 0
	lui	$24, 0
	lui	$25, 0
	lui	$26, 0
	lui	$27, 0
	lui	$28, 0
	lui	$29, 0
	lui	$30, 0
	lui	$31, 0

# Force at least 8 (non-delay-slot) zero bytes, to make 'objdump' print ...
      .space  8
