#as: --abi=64
#objdump: -sr
#source: immexpr2.s
#name: Resolved 64-bit operand, 64-bit ABI.

.*:     file format .*-sh64.*

Contents of section \.text:
 0000 6ff0fff0 6ff0fff0 6ff0fff0           .*
Contents of section .data:
 0000 00000000 00000004 00000000 00000008  .*
