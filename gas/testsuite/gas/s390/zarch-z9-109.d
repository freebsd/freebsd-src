#name: s390x opcode
#objdump: -drw

.*: +file format .*

Disassembly of section .text:

.* <foo>:
.*:	c2 69 80 00 00 00 [	 ]*afi	%r6,-2147483648
.*:	c2 68 80 00 00 00 [	 ]*agfi	%r6,-2147483648
.*:	c2 6b ff ff ff ff [	 ]*alfi	%r6,4294967295
.*:	c2 6a ff ff ff ff [	 ]*algfi	%r6,4294967295
.*:	c0 6a ff ff ff ff [	 ]*nihf	%r6,4294967295
.*:	c0 6b ff ff ff ff [	 ]*nilf	%r6,4294967295
.*:	c2 6d 80 00 00 00 [	 ]*cfi	%r6,-2147483648
.*:	c2 6c 80 00 00 00 [	 ]*cgfi	%r6,-2147483648
.*:	c2 6f ff ff ff ff [	 ]*clfi	%r6,4294967295
.*:	c2 6e ff ff ff ff [	 ]*clgfi	%r6,4294967295
.*:	c0 66 ff ff ff ff [	 ]*xihf	%r6,4294967295
.*:	c0 67 ff ff ff ff [	 ]*xilf	%r6,4294967295
.*:	c0 68 ff ff ff ff [	 ]*iihf	%r6,4294967295
.*:	c0 69 ff ff ff ff [	 ]*iilf	%r6,4294967295
.*:	b9 83 00 69 [	 ]*flogr	%r6,%r9
.*:	e3 65 a0 00 80 12 [	 ]*lt	%r6,-524288\(%r5,%r10\)
.*:	e3 65 a0 00 80 02 [	 ]*ltg	%r6,-524288\(%r5,%r10\)
.*:	b9 26 00 69 [	 ]*lbr	%r6,%r9
.*:	b9 06 00 69 [	 ]*lgbr	%r6,%r9
.*:	b9 27 00 69 [	 ]*lhr	%r6,%r9
.*:	b9 07 00 69 [	 ]*lghr	%r6,%r9
.*:	c0 61 80 00 00 00 [	 ]*lgfi	%r6,-2147483648
.*:	e3 65 a0 00 80 94 [	 ]*llc	%r6,-524288\(%r5,%r10\)
.*:	b9 94 00 69 [	 ]*llcr	%r6,%r9
.*:	b9 84 00 69 [	 ]*llgcr	%r6,%r9
.*:	e3 65 a0 00 80 95 [	 ]*llh	%r6,-524288\(%r5,%r10\)
.*:	b9 95 00 69 [	 ]*llhr	%r6,%r9
.*:	b9 85 00 69 [	 ]*llghr	%r6,%r9
.*:	c0 6e ff ff ff ff [	 ]*llihf	%r6,4294967295
.*:	c0 6f ff ff ff ff [	 ]*llilf	%r6,4294967295
.*:	c0 6c ff ff ff ff [	 ]*oihf	%r6,4294967295
.*:	c0 6d ff ff ff ff [	 ]*oilf	%r6,4294967295
.*:	c2 65 ff ff ff ff [	 ]*slfi	%r6,4294967295
.*:	c2 64 ff ff ff ff [	 ]*slgfi	%r6,4294967295
.*:	b2 b0 5f ff [	 ]*stfle	4095\(%r5\)
.*:	b2 7c 5f ff [	 ]*stckf	4095\(%r5\)
.*:	c8 60 5f ff af ff [	 ]*mvcos	4095\(%r5\),4095\(%r10\),%r6
.*:	b9 aa 5f 69 [	 ]*lptea	%r6,%r9,%r5,15
.*:	b2 2b f0 69 [	 ]*sske	%r6,%r9,15
.*:	b9 b1 f0 69 [	 ]*cu24	%r6,%r9,15
.*:	b2 a6 f0 69 [	 ]*cu21	%r6,%r9,15
.*:	b9 b3 f0 69 [	 ]*cu42	%r6,%r9,15
.*:	b9 b2 f0 69 [	 ]*cu41	%r6,%r9,15
.*:	b2 a7 f0 69 [	 ]*cu12	%r6,%r9,15
.*:	b9 b0 f0 69 [	 ]*cu14	%r6,%r9,15
.*:	b3 3b 60 95 [	 ]*myr	%f6,%f9,%f5
.*:	b3 3d 60 95 [	 ]*myhr	%f6,%f9,%f5
.*:	b3 39 60 95 [	 ]*mylr	%f6,%f9,%f5
.*:	ed 95 af ff 60 3b [	 ]*my	%f6,%f9,4095\(%r5,%r10\)
.*:	ed 95 af ff 60 3d [	 ]*myh	%f6,%f9,4095\(%r5,%r10\)
.*:	ed 95 af ff 60 39 [	 ]*myl	%f6,%f9,4095\(%r5,%r10\)
.*:	b3 3a 60 95 [	 ]*mayr	%f6,%f9,%f5
.*:	b3 3c 60 95 [	 ]*mayhr	%f6,%f9,%f5
.*:	b3 38 60 95 [	 ]*maylr	%f6,%f9,%f5
.*:	ed 95 af ff 60 3a [	 ]*may	%f6,%f9,4095\(%r5,%r10\)
.*:	ed 95 af ff 60 3c [	 ]*mayh	%f6,%f9,4095\(%r5,%r10\)
.*:	ed 95 af ff 60 38 [	 ]*mayl	%f6,%f9,4095\(%r5,%r10\)
