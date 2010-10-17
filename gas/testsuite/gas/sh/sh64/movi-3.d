#as: --abi=64
#objdump: -dr
#source: movi-3.s
#name: Assembler PC-rel resolved negative MOVI.

.*:     file format .*-sh64.*

Disassembly of section \.text:

[0]+ <start>:
[ 	]+0:[ 	]+cef68040[ 	]+movi	-16992,r4
[ 	]+4:[ 	]+cfffc050[ 	]+movi	-16,r5
[ 	]+8:[ 	]+cffffc60[ 	]+movi	-1,r6
[ 	]+c:[ 	]+cffffc70[ 	]+movi	-1,r7
[ 	]+10:[ 	]+cffffc80[ 	]+movi	-1,r8
[ 	]+14:[ 	]+cbfffc80[ 	]+shori	65535,r8
[ 	]+18:[ 	]+cbffc080[ 	]+shori	65520,r8
[ 	]+1c:[ 	]+caf68080[ 	]+shori	48544,r8
