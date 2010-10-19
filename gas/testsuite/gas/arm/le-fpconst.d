#objdump: -s --section=.text
#as: -EL
#name: arm little-endian fpconst
# Not all arm targets are bi-endian, so only run this test on ones
# we know that are.  FIXME We should probably also key off armeb/armel.
#target: *-*-pe

.*: +file format .*arm.*

Contents of section .text:
 0000 cdcc8c3f 00000000 9999f13f 9a999999 .*
