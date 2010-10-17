#objdump: -d --prefix-addresses
#name: padding of .init section

.*: +file format .*

Disassembly of section .init:
00000000 <.init> subu        r31,r31,0x10
00000004 <.init\+0x4> st          r13,r31,0x20
00000008 <.init\+0x8> or          r0,r0,r0
0000000c <.init\+0xc> or          r0,r0,r0
