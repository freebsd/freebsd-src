#objdump: -dr
#as: --em=criself --pic
#name: PIC relocs.

.*:     file format .*-cris

Disassembly of section \.text:

0+ <start>:
[ 	]+0:[ 	]+af1e 0000 0000[ 	]+sub\.d 0 <start>,\$?r1
[ 	]+2:[ 	]+R_CRIS_32_GOTREL	\.text
[ 	]+6:[ 	]+6f3d 0000 0000 6aaa[ 	]+move\.d \[\$?r3\+0 <start>\],\$?r10
[ 	]+8:[ 	]+R_CRIS_32_GOT	extsym
[ 	]+e:[ 	]+2f9e 0000 0000[ 	]+add\.d 0 <start>,\$?r9
[ 	]+10:[ 	]+R_CRIS_32_GOTREL	extsym2
[ 	]+14:[ 	]+6f8e 0000 0000[ 	]+move\.d 0 <start>,\$?r8
[ 	]+16:[ 	]+R_CRIS_32_PLT_PCREL	extsym5
[ 	]+1a:[ 	]+6f8e 0000 0000[ 	]+move\.d 0 <start>,\$?r8
[ 	]+1c:[ 	]+R_CRIS_32_PLT_GOTREL[ 	]+extsym9
[ 	]+20:[ 	]+6f3d 0000 0000 6aaa[ 	]+move\.d \[\$?r3\+0 <start>\],\$?r10
[ 	]+22:[ 	]+R_CRIS_32_GOTPLT	extsym
[ 	]+28:[ 	]+5fdd 0000 6aaa[ 	]+move\.d \[\$?r13\+0\],\$?r10
[ 	]+2a:[ 	]+R_CRIS_16_GOT	extsym13
[ 	]+2e:[ 	]+5fae 0000[ 	]+move\.w 0x0,\$?r10
[ 	]+30:[ 	]+R_CRIS_16_GOTPLT	extsym14
[ 	]+32:[ 	]+6f3d 0000 0000 aa4a[ 	]+sub\.d \[\$?r3\+0 <start>\],\$?r4,\$?r10
[ 	]+34:[ 	]+R_CRIS_32_GOT	extsym3
[ 	]+3a:[ 	]+af9e 0000 0000[ 	]+sub\.d 0 <start>,\$?r9
[ 	]+3c:[ 	]+R_CRIS_32_GOTREL	extsym4\+0x2a
[ 	]+40:[ 	]+af3e 0000 0000[ 	]+sub\.d 0 <start>,\$?r3
[ 	]+42:[ 	]+R_CRIS_32_GOTREL	extsym4\+0x[f]+fffffa0
[ 	]+46:[ 	]+6fad 0000 0000 287a[ 	]+add\.d \[\$?r10\+0 <start>\],\$?r7,\$?r8
[ 	]+48:[ 	]+R_CRIS_32_GOT	extsym3\+0x38
[ 	]+4e:[ 	]+6f5d 0000 0000 611a[ 	]+move\.d \[\$?r5\+0 <start>\],\$?r1
[ 	]+50:[ 	]+R_CRIS_32_GOT	extsym6\+0xa
[ 	]+56:[ 	]+6fad 0000 0000 284a[ 	]+add\.d \[\$?r10\+0 <start>\],\$?r4,\$?r8
[ 	]+58:[ 	]+R_CRIS_32_GOT	extsym3\+0x[f]+ffffdd0
[ 	]+5e:[ 	]+6f5d 0000 0000 6cca[ 	]+move\.d \[\$?r5\+0 <start>\],\$?r12
[ 	]+60:[ 	]+R_CRIS_32_GOT	extsym6\+0x[f]+fffff92
[ 	]+66:[ 	]+6f5d 0000 0000 69ce[ 	]+move\.d \[\$?r9=\$?r5\+0 <start>\],\$?r12
[ 	]+68:[ 	]+R_CRIS_32_GOT	extsym6\+0x[f]+fffff24
[ 	]+6e:[ 	]+6f3d 0000 0000 67de[ 	]+move\.d \[\$?r7=\$?r3\+0 <start>\],\$?r13
[ 	]+70:[ 	]+R_CRIS_32_GOTREL	extsym10\+0x[f]+ffffeb6
[ 	]+76:[ 	]+6f5e 0000 0000[ 	]+move\.d 0 <start>,\$?r5
[ 	]+78:[ 	]+R_CRIS_32_PLT_PCREL	extsym7\+0x4
[ 	]+7c:[ 	]+6f9e 0000 0000[ 	]+move\.d 0 <start>,\$?r9
[ 	]+7e:[ 	]+R_CRIS_32_PLT_PCREL	extsym7\+0x[f]+fffffd8
[ 	]+82:[ 	]+6f5e 0000 0000[ 	]+move\.d 0 <start>,\$?r5
[ 	]+84:[ 	]+R_CRIS_32_PLT_GOTREL	extsym11\+0x10
[ 	]+88:[ 	]+6f9e 0000 0000[ 	]+move\.d 0 <start>,\$?r9
[ 	]+8a:[ 	]+R_CRIS_32_PLT_GOTREL	extsym12\+0x[f]+fffffc4
[ 	]+8e:[ 	]+5fcd 0000 a89a[ 	]+sub\.d \[\$?r12\+0\],\$?r9,\$?r8
[ 	]+90:[ 	]+R_CRIS_16_GOT	extsym3\+0x[f]+fffff64
[ 	]+94:[ 	]+5fbd 0000 699a[ 	]+move\.d \[\$?r11\+0\],\$?r9
[ 	]+96:[ 	]+R_CRIS_16_GOTPLT	extsym14\+0x[f]+fffff00
[ 	]+9a:[ 	]+6fad 0000 0000 287a[ 	]+add\.d \[\$?r10\+0 <start>\],\$?r7,\$?r8
[ 	]+9c:[ 	]+R_CRIS_32_GOTPLT	extsym3\+0x38
[ 	]+\.\.\.
