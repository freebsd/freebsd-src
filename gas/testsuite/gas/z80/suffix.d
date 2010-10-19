#objdump: -s -r -j .data
#name: suffixes

.*:.*

RELOCATION RECORDS FOR \[.data\]:
OFFSET[ 	]+TYPE[ 	]+VALUE[ 	]*
00000002[ 	]+r_imm16[ 	]+.data[ 	]*
00000014[ 	]+r_imm16[ 	]+.data[ 	]*


Contents of section .data:
 0000 0a000000 08020802 08020802 f203f203[ 	]+................[ 	]*
 0010 10b010b0 1600[ 	]+......[ 	]*
#pass
