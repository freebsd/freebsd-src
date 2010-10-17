#as: --abi=32 --isa=shmedia -gdwarf2
#objdump: -dl
#source: lineno.s
#name: Dwarf2 line numbers vs macro opcodes

.*:     file format .*-sh64.*

Disassembly of section .text:

[0]+ <start>:
start.*:
[	 ]+0:[	 ]+cc000410[	 ]+movi[	 ]+1,r1
.*:4
[	 ]+4:[	 ]+cc000410[	 ]+movi[	 ]+1,r1
.*:5
[	 ]+8:[	 ]+ca1a8010[	 ]+shori[	 ]+34464,r1
[	 ]+c:[	 ]+6ff0fff0[	 ]+nop[	 ]*
.*:6
[	 ]+10:[	 ]+6ff0fff0[	 ]+nop[	 ]*
