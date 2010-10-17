#objdump: -dr
#as: --no-mul-bug-abort
#name: @OC@

# Test the @OC@ insn.

.*:     file format .*-cris

Disassembly of section \.text:
0+ <start>:
[	 ]+0:[	 ]+@IR+4134@[ 	]+@OC@[ ]+\$?r1,\$?r3
[	 ]+2:[	 ]+@IR+4004@[ 	]+@OC@[ ]+\$?r0,\$?r0
[	 ]+4:[	 ]+@IR+40d4@[ 	]+@OC@[ ]+\$?r0,\$?r13
[	 ]+6:[	 ]+@IR+4504@[ 	]+@OC@[ ]+\$?r5,\$?r0
[	 ]+8:[	 ]+@IR+4dd4@[ 	]+@OC@[ ]+\$?r13,\$?r13
[	 ]+a:[	 ]+@IR+4934@[ 	]+@OC@[ ]+\$?r9,\$?r3
