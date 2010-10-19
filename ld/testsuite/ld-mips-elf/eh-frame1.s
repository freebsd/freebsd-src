#----------------------------------------------------------------------------
# Macros
#----------------------------------------------------------------------------

	mask = (1 << alignment) - 1

	# Output VALUE as an unaligned pointer-sized quantity.
	.macro pbyte value
	.if alignment == 2
	.4byte		\value
	.else
	.8byte		\value
	.endif
	.endm


	# Start a new CIE, and emit everything up to the augmentation data.
	# Use LABEL to mark the start of the entry and AUG as the augmentation
	# string.
	.macro start_cie label,aug
	.section	.eh_frame,"aw",@progbits
\label:
	.word		2f-1f		# Length
1:
	.word		0		# Identifier
	.byte		1		# Version
	.string		"\aug"		# Augmentation
	.byte		1		# Code alignment
	.byte		4		# Data alignment
	.byte		31		# Return address column
	.endm


	# Create a dummy function of SIZE bytes in SECTION and emit the
	# first four entries of an FDE for it.
	.macro start_fde cie,section,size
	.section	\section,"ax",@progbits
3:
	.rept		\size / 4
	nop
	.endr
4:
	.section	.eh_frame,"aw",@progbits
	.word		2f-1f		# Length
1:
	.word		.-\cie		# CIE offset
	pbyte		3b		# Initial PC
	pbyte		4b-3b		# Size of code
	.endm


	# Finish a CIE or FDE entry.
	.macro end_entry
	.p2align	alignment,fill
2:
	.endm


	# Start the augmentation data for a CIE that has a 'P' entry
	# followed by EXTRA bytes.  AUGLEN is the length of augmentation
	# string (including zero terminator), ENCODING is the encoding to
	# use for the personality routine and VALUE is the value it
	# should have.
	.macro		persaug auglen,extra,encoding,value
	.if (\encoding & 0xf0) == 0x50
	.byte		(-(9 + \auglen + 3 + 2) & mask) + 2 + mask + \extra
	.byte		\encoding
	.fill		-(9 + \auglen + 3 + 2) & mask,1,0
	.else
	.byte		2 + mask + \extra
	.byte		\encoding
	.endif
	pbyte		\value
	.endm


	.macro cie_basic label
	start_cie	\label,""
	end_entry
	.endm

	.macro fde_basic cie,section,size
	start_fde	\cie,\section,\size
	end_entry
	.endm


	.macro cie_zP label,encoding,value
	start_cie	 \label,"zP"
	persaug		3,0,\encoding,\value
	end_entry
	.endm

	.macro fde_zP cie,section,size
	start_fde	 \cie,\section,\size
	.byte		 0		# Augmentation length
	end_entry
	.endm


	.macro cie_zPR label,encoding,value
	start_cie	 \label,"zPR"
	persaug		4,1,\encoding,\value
	.byte		0		# FDE enconding
	end_entry
	.endm

	.macro fde_zPR cie,section,size
	start_fde	\cie,\section,\size
	.byte		0		# Augmentation length
	end_entry
	.endm

#----------------------------------------------------------------------------
# Test code
#----------------------------------------------------------------------------

	cie_basic	basic1
	fde_basic	basic1,.text,0x10
	fde_basic	basic1,.text,0x20

	cie_basic	basic2
	fde_basic	basic2,.text,0x30

	cie_basic	basic3
	fde_basic	basic3,.text,0x40

	cie_basic	basic4
	fde_basic	basic4,.text,0x50

	cie_zP		zP_unalign1,0x00,foo
	fde_zP		zP_unalign1,.text,0x10
	fde_zP		zP_unalign1,.text,0x20

	cie_zP		zP_align1,0x50,foo
	fde_zP		zP_align1,.text,0x10
	fde_zP		zP_align1,.text,0x20

	cie_zPR		zPR1,0x00,foo
	fde_zPR		zPR1,.text,0x10
	fde_zPR		zPR1,.discard,0x20

	cie_zPR		zPR2,0x00,foo
	fde_zPR		zPR2,.text,0x30
	fde_zPR		zPR2,.text,0x40

	cie_basic	basic5
	fde_basic	basic5,.text,0x10

	.if alignment == 2
	.section	.gcc_compiled_long32
	.endif
