#source: stobin.s
#as: --abi=32 --isa=SHmedia
#ld: -mshelf32 tmpdir/stobin-0-dso.so
#objdump: -drj.text
#target: sh64-*-elf

.*: +file format elf32-sh64.*

Disassembly of section \.text:

0+[0-9a-f]+ <start>:
    [0-9a-f]+:	cffffd90 	movi	-1,r25
    [0-9a-f]+:	cbfee590 	shori	65465,r25	! 0xffffffb9 .*
    [0-9a-f]+:	6bf56600 	ptrel/l	r25,tr0
    [0-9a-f]+:	4401fff0 	blink	tr0,r63
