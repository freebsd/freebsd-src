#objdump: -dr
#name: unimplemented

.*:     file format .*-cris

Disassembly of section \.text:
0+ <start>:
[	 ]+0:[	 ]+bb2a[ 	]+bmod[ ]+\[\$?r11\],\$?r2
[	 ]+2:[	 ]+4b35 b22a[ 	]+bmod[ ]+\[\$?r11\+\$?r3\.b\],\$?r2
[	 ]+6:[	 ]+6355 bd2e[ 	]+bmod[ ]+\[\$?r13=\$?r3\+\$?r5\.d\],\$?r2
[	 ]+a:[	 ]+6fbd 0000 0000 b22a[ 	]+bmod[ ]+\[\$?r11\+0( <start>)?\],\$?r2
[ 	]+c:[ 	]+(R_CRIS_)?32[ 	]+external_symbol
[	 ]+12:[	 ]+7f0d 0000 0000 b22a[ 	]+bmod[ ]+\[(0x0|0 <start>)?\],\$?r2
[ 	]+14:[ 	]+(R_CRIS_)?32[ 	]+external_symbol
[	 ]+1a:[	 ]+fb2a[ 	]+bstore[ ]+\[\$?r11\],\$?r2
[	 ]+1c:[	 ]+4b35 f22a[ 	]+bstore[ ]+\[\$?r11\+\$?r3\.b\],\$?r2
[	 ]+20:[	 ]+6355 fd2e[ 	]+bstore[ ]+\[\$?r13=\$?r3\+\$?r5\.d\],\$?r2
[	 ]+24:[	 ]+6fbd 0000 0000 f22a[ 	]+bstore[ ]+\[\$?r11\+0( <start>)?\],\$?r2
[ 	]+26:[ 	]+(R_CRIS_)?32[ 	]+external_symbol
[	 ]+2c:[	 ]+7f0d 0000 0000 f22a[ 	]+bstore[ ]+\[(0x0|0 <start>)?\],\$?r2
[ 	]+2e:[ 	]+(R_CRIS_)?32[ 	]+external_symbol
[	 ]+34:[	 ]+8889[ 	]+div.b \$?r8,\$?r8
[	 ]+36:[	 ]+8749[ 	]+div.b \$?r4,\$?r7
[	 ]+38:[	 ]+8009[ 	]+div.b \$?r0,\$?r0
[	 ]+3a:[	 ]+8449[ 	]+div.b \$?r4,\$?r4
[	 ]+3c:[	 ]+9749[ 	]+div.w \$?r4,\$?r7
[	 ]+3e:[	 ]+9009[ 	]+div.w \$?r0,\$?r0
[	 ]+40:[	 ]+9449[ 	]+div.w \$?r4,\$?r4
[	 ]+42:[	 ]+a749[ 	]+div.d \$?r4,\$?r7
[	 ]+44:[	 ]+a009[ 	]+div.d \$?r0,\$?r0
[	 ]+46:[	 ]+a449[ 	]+div.d \$?r4,\$?r4
