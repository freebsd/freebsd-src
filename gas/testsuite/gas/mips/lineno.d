#objdump: -d -l -mmips:4000
#name: assembly line numbers
#as: --gstabs -march=r4000


.*: +file format .*mips.*

Disassembly of section \.text:
0+0000 <main-0x10>:
.*[0-9a-f]+:.*deadbeef.*
.*[0-9a-f]+:.*deadbeef.*
.*[0-9a-f]+:.*deadbeef.*
.*[0-9a-f]+:.*deadbeef.*

0+0010 <main>:
main\(\):
.*lineno.s:16
.*10:.*addiu.*
.*lineno.s:17
.*14:.*sd.*
.*lineno.s:18
.*18:.*sd.*
.*lineno.s:19
.*1c:.*move.*
.*lineno.s:20
.*20:.*jal.*
.*24:.*nop
.*lineno.s:21
.*28:.*li.*
.*lineno.s:22
.*2c:.*sw.*
.*lineno.s:23
.*30:.*lw.*
.*lineno.s:24
.*34:.*move.*
.*lineno.s:25
.*38:.*sll.*
.*lineno.s:26
.*3c:.*addu.*
.*lineno.s:27
.*40:.*sw.*
.*lineno.s:28
.*44:.*lw.*
.*lineno.s:29
.*48:.*jal.*
.*4c:.*nop
.*lineno.s:30
.*50:.*lw.*
.*lineno.s:31
.*54:.*move.*
.*lineno.s:32
.*58:.*b.*
.*5c:.*nop
# Objdump puts an '$L1' symbol here for ELF, but not for ECOFF.  For
# ELF, $L1 is in the .text section so objdump picks it for this location.
# For ECOFF, it's in the *DEBUG* section, so objdump prefers the .text
# symbol over $L1.
#...
.*lineno.s:34
.*60:.*move.*
.*lineno.s:35
.*64:.*ld.*
.*lineno.s:36
.*68:.*ld.*
.*lineno.s:37
.*6c:.*addiu.*
.*lineno.s:38
.*70:.*jr.*
.*74:.*nop

0+0078 <g>:
g\(\):
.*lineno.s:47
.*78:.*addiu.*
.*lineno.s:48
.*7c:.*sd.*
.*lineno.s:49
.*80:.*move.*
.*lineno.s:50
.*84:.*sw.*
.*lineno.s:51
.*88:.*lw.*
.*lineno.s:52
.*8c:.*addiu.*
.*lineno.s:53
.*90:.*move.*
.*lineno.s:54
.*94:.*b.*
.*98:.*nop
# Objdump puts an '$L2' symbol here for ELF, but not for ECOFF.
#...
.*lineno.s:56
.*9c:.*move.*
.*lineno.s:57
.*a0:.*ld.*
.*lineno.s:58
.*a4:.*addiu.*
.*lineno.s:59
.*a8:.*jr.*
.*ac:.*nop
