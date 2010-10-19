# name: bignums
# as:
# objdump: --full-contents
# This test is only valid on ELF based ports.
#not-target: *-*-*coff *-*-pe *-*-wince *-*-*aout* *-*-netbsd *-*-riscix*

.*: +file format .*arm.*

Contents of section .data:
 0000 [08]0000000 000000[08]0 11111111 11111111  \.\.\.\.\.\.\.\.\.\.\.\.\.\.\.\.
# Ignore .ARM.attributes section
#...
