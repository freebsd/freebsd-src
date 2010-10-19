#objdump: -dr --prefix-addresses --show-raw-insn
#as: --allow-reg-prefix -little
#name: SH --allow-reg-prefix option
# Test SH register names prefixed with $:

.*:     file format elf.*sh.*

Disassembly of section .text:
0x00000000 12 60       	mov\.l	@r1,r0

