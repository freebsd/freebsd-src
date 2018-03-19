Getting Started {#start}
========================

<!---
 ! Copyright (c) 2013-2018, Intel Corporation
 !
 ! Redistribution and use in source and binary forms, with or without
 ! modification, are permitted provided that the following conditions are met:
 !
 !  * Redistributions of source code must retain the above copyright notice,
 !    this list of conditions and the following disclaimer.
 !  * Redistributions in binary form must reproduce the above copyright notice,
 !    this list of conditions and the following disclaimer in the documentation
 !    and/or other materials provided with the distribution.
 !  * Neither the name of Intel Corporation nor the names of its contributors
 !    may be used to endorse or promote products derived from this software
 !    without specific prior written permission.
 !
 ! THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 ! AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 ! IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 ! ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 ! LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 ! CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 ! SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 ! INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 ! CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 ! ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 ! POSSIBILITY OF SUCH DAMAGE.
 !-->

This chapter gives a brief introduction into the sample tools using one of the
tests as example.  It assumes that you are already familiar with Intel(R)
Processor Trace (Intel PT) and that you already built the decoder library and
the sample tools.  For detailed information about Intel PT, please refer to
chapter 11 of the Intel Architecture Instruction Set Extensions Programming
Reference at http://www.intel.com/products/processor/manuals/.

Start by compiling the loop-tnt test.  It consists of a small assembly program
with interleaved Intel PT directives:

	$ pttc test/src/loop-tnt.ptt
	loop-tnt-ptxed.exp
	loop-tnt-ptdump.exp

This produces the following output files:

	loop-tnt.lst          a yasm assembly listing file
	loop-tnt.bin          a raw binary file
	loop-tnt.pt           a Intel PT file
	loop-tnt-ptxed.exp    the expected ptxed output
	loop-tnt-ptdump.exp   the expected ptdump output

The latter two files are generated based on the `@pt .exp(<tool>)` directives
found in the `.ptt` file.  They are used for automated testing.  See
script/test.bash for details on that.


Use `ptdump` to dump the Intel PT packets:

	$ ptdump loop-tnt.pt
	0000000000000000  psb
	0000000000000010  fup        3: 0x0000000000100000, ip=0x0000000000100000
	0000000000000017  mode.exec  cs.d=0, cs.l=1 (64-bit mode)
	0000000000000019  psbend
	000000000000001b  tnt8       !!.
	000000000000001c  tip.pgd    3: 0x0000000000100013, ip=0x0000000000100013

The ptdump tool takes an Intel PT file as input and dumps the packets in
human-readable form.  The number on the very left is the offset into the Intel
PT packet stream in hex.  This is followed by the packet opcode and payload.


Use `ptxed` for reconstructing the execution flow.  For this, you need the Intel
PT file as well as the corresponding binary image.  You need to specify the load
address given by the org directive in the .ptt file when using a raw binary
file.

	$ ptxed --pt loop-tnt.pt --raw loop-tnt.bin:0x100000
	0x0000000000100000  mov rax, 0x0
	0x0000000000100007  jmp 0x10000d
	0x000000000010000d  cmp rax, 0x1
	0x0000000000100011  jle 0x100009
	0x0000000000100009  add rax, 0x1
	0x000000000010000d  cmp rax, 0x1
	0x0000000000100011  jle 0x100009
	0x0000000000100009  add rax, 0x1
	0x000000000010000d  cmp rax, 0x1
	0x0000000000100011  jle 0x100009
	[disabled]

Ptxed prints disassembled instructions in execution order as well as status
messages enclosed in brackets.
