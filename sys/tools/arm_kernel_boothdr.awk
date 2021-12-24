#!/usr/bin/awk -f
#-
# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
#
# Copyright 2019 Ian Lepore <ian@freebsd.org>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# $FreeBSD$

BEGIN {
	# Init global vars.
	gBytesOut = 0;  # How many output bytes we've written so far
	gKernbase = 0;  # Address of first byte of loaded kernel image
	gStart = 0;     # Address of _start symbol
	gStartOff = 0;  # Offset of _start symbol from start of image
	gEnd = 0;       # Address of _end symbol
	gEndOff = 0;    # Offset of _end symbol from start of image

	# The type of header we're writing is set using -v hdrtype= on
	# the command line, ensure we got a valid value for it.
	if (hdrtype != "v7jump" &&
	    hdrtype != "v8jump" &&
	    hdrtype != "v8booti") {
		print "arm_kernel_boothdr.awk: " \
		    "missing or invalid '-v hdrtype=' argument" >"/dev/stderr"
		gHdrType = "error_reported"
		exit 1
	}

	gHdrType = hdrtype
}

function addr_to_offset(addr) {
	# Turn an address into an offset from the start of the loaded image.
	return addr % gKernbase
}

function hexstr_to_num(str) {

	# Prepend a 0x onto the string, then coerce it to a number by doing
	# arithmetic with it, which makes awk run it through strtod(),
	# which handles hex numbers that have a 0x prefix.

	return 0 + ("0x" str)
}

function write_le32(num) {

	for (i = 0; i < 4; i++) {
		printf("%c", num % 256);
		num /= 256
	}
	gBytesOut += 4
}

function write_le64(num) {

	for (i = 0; i < 8; i++) {
		printf("%c", num % 256);
		num /= 256
	}
	gBytesOut += 8
}

function write_padding() {

	# Write enough padding bytes so that the header fills all the
	# remaining space before the _start symbol.

	while (gBytesOut++ < gStartOff) {
		printf("%c", 0);
	}
}

function write_v7jump() {

	# Write the machine code for "b _start"...
	#   0xea is armv7 "branch always" and the low 24 bits is the signed
	#   offset from the current PC, in words.  We know the gStart offset
	#   is in the first 2mb, so it'll fit in 24 bits.

	write_le32(hexstr_to_num("ea000000") + (gStartOff / 4) - 2)
}

function write_v8jump() {

	# Write the machine code for "b _start"...
	#   0x14 is armv8 "branch always" and the low 26 bits is the signed
	#   offset from the current PC, in words.  We know the gStart offset
	#   is in the first 2mb, so it'll fit in 26 bits.

	write_le32(hexstr_to_num("14000000") + (gStartOff / 4))
}

function write_v8booti() {

	# We are writing this struct...
	#
	# struct Image_header {
	#	uint32_t	code0;		/* Executable code */
	#	uint32_t	code1;		/* Executable code */
	#	uint64_t	text_offset;	/* Image load offset, LE */
	#	uint64_t	image_size;	/* Effective Image size, LE */
	#	uint64_t	flags;		/* Kernel flags, LE */
	#	uint64_t	res1[3];	/* reserved */
	#	uint32_t	magic;		/* Magic number */
	#	uint32_t	res2;
	# };
	#
	# We write 'b _start' into code0.  The image size is everything from
	# the start of the loaded image to the offset given by the _end symbol.

	write_v8jump()                        # code0
	write_le32(0)                         # code1
	write_le64(0)                         # text_offset
	write_le64(gEndOff)                   # image_size
	write_le64(hexstr_to_num("8"))        # flags
	write_le64(0)                         # res1[0]
	write_le64(0)                         # res1[1]
	write_le64(0)                         # res1[2]
	write_le32(hexstr_to_num("644d5241")) # magic (LE "ARMd" (d is 0x64))
	write_le32(0)                         # res2
}

/kernbase/ {
	# If the symbol name is exactly "kernbase" save its address.
	if ($3 == "kernbase") {
		gKernbase = hexstr_to_num($1)
	}
}

/_start/ {
	# If the symbol name is exactly "_start" save its address.
	if ($3 == "_start") {
		gStart = hexstr_to_num($1)
	}
}

/_end/ {
	# If the symbol name is exactly "_end" remember its value.
	if ($3 == "_end") {
		gEnd = hexstr_to_num($1)
	}
}

END {
	# Note that this function runs even if BEGIN calls exit(1)!
	if (gHdrType == "error_reported") {
		exit 1
	}

	# Make sure we got all three required symbols.
	if (gKernbase == 0 || gStart == 0 || gEnd == 0) {
		print "arm_kernel_boothdr.awk: " \
		    "missing kernbase/_start/_end symbol(s)" >"/dev/stderr"
		    exit 1
	}

	gStartOff = addr_to_offset(gStart)
	gEndOff = addr_to_offset(gEnd)

	if (gHdrType == "v7jump") {
		write_v7jump()
	} else if (gHdrType == "v8jump") {
		write_v8jump()
	} else if (gHdrType == "v8booti") {
		write_v8booti()
	}
	write_padding()
}
