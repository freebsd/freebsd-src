#
# Copyright (c) 2000 Jonathan Lemon
# All rights reserved.
#
# Redistribution and use in source and binary forms are freely
# permitted provided that the above copyright notice and this
# paragraph and the following disclaimer are duplicated in all
# such forms.
#
# This software is provided "AS IS" and without any express or
# implied warranties, including, without limitation, the implied
# warranties of merchantability and fitness for a particular
# purpose.
#
# $FreeBSD: src/sys/boot/i386/liloldr/lilobsect.s,v 1.1 2000/07/17 17:06:04 jlemon Exp $

#
# LILO constants
#
		.set SETUP_OFF,497		# offset of setup table
		.set SETUP_SECTORS,4		# historical

		.globl start
		.org 0x0, 0x0
#
# Create an empty bootblock, but fill in the setup table.
#
bootsect:
		.org SETUP_OFF,0x00
#
# bootblock setup for LILO
#
		.byte SETUP_SECTORS		# size of setup code in sectors
		.word 0x00			# read only root
		.word LOADER_SIZE		# size of kernel in 16B units
		.word 0x00			# not used (swap dev?)
		.word 0x00			# ram disk size in KB
		.word 0xffff			# video mode (80x25)
		.byte 0x00			# root dev major number
		.byte 0x00			# root dev minor number
		.word 0xaa55			# Magic number
