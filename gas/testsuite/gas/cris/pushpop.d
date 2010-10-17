#objdump: -dr
#name: pushpop

.*:[ 	]+file format .*-cris

Disassembly of section \.text:
0+ <start>:
[	 ]+0:[	 ]+fce1 ee1f[ 	]+push[ ]+\$?r1
[	 ]+4:[	 ]+fce1 ee0f[ 	]+push[ ]+\$?r0
[	 ]+8:[	 ]+fce1 ee4f[ 	]+push[ ]+\$?r4
[	 ]+c:[	 ]+f8e1 ce5f[ 	]+move\.b \$?r5,\[\$?sp=\$?sp-8\]
[	 ]+10:[	 ]+f8e1 de5f[ 	]+move\.w \$?r5,\[\$?sp=\$?sp-8\]
[	 ]+14:[	 ]+f8e1 ee5f[ 	]+move\.d \$?r5,\[\$?sp=\$?sp-8\]
[	 ]+18:[	 ]+fbe1 ce5f[ 	]+move\.b \$?r5,\[\$?sp=\$?sp-5\]
[	 ]+1c:[	 ]+fbe1 de5f[ 	]+move\.w \$?r5,\[\$?sp=\$?sp-5\]
[	 ]+20:[	 ]+fbe1 ee5f[ 	]+move\.d \$?r5,\[\$?sp=\$?sp-5\]
[	 ]+24:[	 ]+fce1 de5f[ 	]+move\.w \$?r5,\[\$?sp=\$?sp-4\]
[	 ]+28:[	 ]+fce1 ce3f[ 	]+move\.b \$?r3,\[\$?sp=\$?sp-4\]
[	 ]+2c:[	 ]+fde1 ee5f[ 	]+move\.d \$?r5,\[\$?sp=\$?sp-3\]
[	 ]+30:[	 ]+fde1 debf[ 	]+move\.w \$?r11,\[\$?sp=\$?sp-3\]
[	 ]+34:[	 ]+fde1 ce5f[ 	]+move\.b \$?r5,\[\$?sp=\$?sp-3\]
[	 ]+38:[	 ]+fee1 ee5f[ 	]+move\.d \$?r5,\[\$?sp=\$?sp-2\]
[	 ]+3c:[	 ]+fee1 ce5f[ 	]+move\.b \$?r5,\[\$?sp=\$?sp-2\]
[	 ]+40:[	 ]+ffe1 ee5f[ 	]+move\.d \$?r5,\[\$?sp=\$?sp-1\]
[	 ]+44:[	 ]+ffe1 de5f[ 	]+move\.w \$?r5,\[\$?sp=\$?sp-1\]
[	 ]+48:[	 ]+00e1 ee5f[ 	]+move\.d \$?r5,\[\$?sp=\$?sp\+0\]
[	 ]+4c:[	 ]+00e1 ce5f[ 	]+move\.b \$?r5,\[\$?sp=\$?sp\+0\]
[	 ]+50:[	 ]+00e1 de5f[ 	]+move\.w \$?r5,\[\$?sp=\$?sp\+0\]
[	 ]+54:[	 ]+01e1 ee5f[ 	]+move\.d \$?r5,\[\$?sp=\$?sp\+1\]
[	 ]+58:[	 ]+01e1 de5f[ 	]+move\.w \$?r5,\[\$?sp=\$?sp\+1\]
[	 ]+5c:[	 ]+01e1 ce5f[ 	]+move\.b \$?r5,\[\$?sp=\$?sp\+1\]
[	 ]+60:[	 ]+02e1 ee5f[ 	]+move\.d \$?r5,\[\$?sp=\$?sp\+2\]
[	 ]+64:[	 ]+02e1 de5f[ 	]+move\.w \$?r5,\[\$?sp=\$?sp\+2\]
[	 ]+68:[	 ]+02e1 ce5f[ 	]+move\.b \$?r5,\[\$?sp=\$?sp\+2\]
[	 ]+6c:[	 ]+03e1 ee5f[ 	]+move\.d \$?r5,\[\$?sp=\$?sp\+3\]
[	 ]+70:[	 ]+03e1 de5f[ 	]+move\.w \$?r5,\[\$?sp=\$?sp\+3\]
[	 ]+74:[	 ]+03e1 ce5f[ 	]+move\.b \$?r5,\[\$?sp=\$?sp\+3\]
[	 ]+78:[	 ]+04e1 ee5f[ 	]+move\.d \$?r5,\[\$?sp=\$?sp\+4\]
[	 ]+7c:[	 ]+04e1 de5f[ 	]+move\.w \$?r5,\[\$?sp=\$?sp\+4\]
[	 ]+80:[	 ]+04e1 ce5f[ 	]+move\.b \$?r5,\[\$?sp=\$?sp\+4\]
[	 ]+84:[	 ]+05e1 ee5f[ 	]+move\.d \$?r5,\[\$?sp=\$?sp\+5\]
[	 ]+88:[	 ]+05e1 de5f[ 	]+move\.w \$?r5,\[\$?sp=\$?sp\+5\]
[	 ]+8c:[	 ]+05e1 ce5f[ 	]+move\.b \$?r5,\[\$?sp=\$?sp\+5\]
[	 ]+90:[	 ]+08e1 ee1f[ 	]+move\.d \$?r1,\[\$?sp=\$?sp\+8\]
[	 ]+94:[	 ]+08e1 de9f[ 	]+move\.w \$?r9,\[\$?sp=\$?sp\+8\]
[	 ]+98:[	 ]+08e1 cedf[ 	]+move\.b \$?r13,\[\$?sp=\$?sp\+8\]
[	 ]+9c:[	 ]+08e1 4e5e[ 	]+move\.b \[\$?sp=\$?sp\+8\],\$?r5
[	 ]+a0:[	 ]+08e1 5e5e[ 	]+move\.w \[\$?sp=\$?sp\+8\],\$?r5
[	 ]+a4:[	 ]+08e1 6e5e[ 	]+move\.d \[\$?sp=\$?sp\+8\],\$?r5
[	 ]+a8:[	 ]+05e1 4e5e[ 	]+move\.b \[\$?sp=\$?sp\+5\],\$?r5
[	 ]+ac:[	 ]+05e1 5e5e[ 	]+move\.w \[\$?sp=\$?sp\+5\],\$?r5
[	 ]+b0:[	 ]+05e1 6e5e[ 	]+move\.d \[\$?sp=\$?sp\+5\],\$?r5
[	 ]+b4:[	 ]+04e1 6e5e[ 	]+move\.d \[\$?sp=\$?sp\+4\],\$?r5
[	 ]+b8:[	 ]+04e1 5e5e[ 	]+move\.w \[\$?sp=\$?sp\+4\],\$?r5
[	 ]+bc:[	 ]+04e1 4e3e[ 	]+move\.b \[\$?sp=\$?sp\+4\],\$?r3
[	 ]+c0:[	 ]+03e1 6e5e[ 	]+move\.d \[\$?sp=\$?sp\+3\],\$?r5
[	 ]+c4:[	 ]+03e1 5ebe[ 	]+move\.w \[\$?sp=\$?sp\+3\],\$?r11
[	 ]+c8:[	 ]+03e1 4e5e[ 	]+move\.b \[\$?sp=\$?sp\+3\],\$?r5
[	 ]+cc:[	 ]+02e1 6e5e[ 	]+move\.d \[\$?sp=\$?sp\+2\],\$?r5
[	 ]+d0:[	 ]+02e1 5e5e[ 	]+move\.w \[\$?sp=\$?sp\+2\],\$?r5
[	 ]+d4:[	 ]+02e1 4e5e[ 	]+move\.b \[\$?sp=\$?sp\+2\],\$?r5
[	 ]+d8:[	 ]+01e1 6e5e[ 	]+move\.d \[\$?sp=\$?sp\+1\],\$?r5
[	 ]+dc:[	 ]+01e1 5e5e[ 	]+move\.w \[\$?sp=\$?sp\+1\],\$?r5
[	 ]+e0:[	 ]+01e1 4e5e[ 	]+move\.b \[\$?sp=\$?sp\+1\],\$?r5
[	 ]+e4:[	 ]+00e1 6e5e[ 	]+move\.d \[\$?sp=\$?sp\+0\],\$?r5
[	 ]+e8:[	 ]+00e1 5e5e[ 	]+move\.w \[\$?sp=\$?sp\+0\],\$?r5
[	 ]+ec:[	 ]+00e1 4e5e[ 	]+move\.b \[\$?sp=\$?sp\+0\],\$?r5
[	 ]+f0:[	 ]+ffe1 6e5e[ 	]+move\.d \[\$?sp=\$?sp-1\],\$?r5
[	 ]+f4:[	 ]+ffe1 5e5e[ 	]+move\.w \[\$?sp=\$?sp-1\],\$?r5
[	 ]+f8:[	 ]+ffe1 4e5e[ 	]+move\.b \[\$?sp=\$?sp-1\],\$?r5
[	 ]+fc:[	 ]+fee1 6e5e[ 	]+move\.d \[\$?sp=\$?sp-2\],\$?r5
[	 ]+100:[	 ]+fee1 5e5e[ 	]+move\.w \[\$?sp=\$?sp-2\],\$?r5
[	 ]+104:[	 ]+fee1 4e5e[ 	]+move\.b \[\$?sp=\$?sp-2\],\$?r5
[	 ]+108:[	 ]+fde1 6e5e[ 	]+move\.d \[\$?sp=\$?sp-3\],\$?r5
[	 ]+10c:[	 ]+fde1 5e5e[ 	]+move\.w \[\$?sp=\$?sp-3\],\$?r5
[	 ]+110:[	 ]+fde1 4e5e[ 	]+move\.b \[\$?sp=\$?sp-3\],\$?r5
[	 ]+114:[	 ]+fce1 6e5e[ 	]+move\.d \[\$?sp=\$?sp-4\],\$?r5
[	 ]+118:[	 ]+fce1 5e5e[ 	]+move\.w \[\$?sp=\$?sp-4\],\$?r5
[	 ]+11c:[	 ]+fce1 4e5e[ 	]+move\.b \[\$?sp=\$?sp-4\],\$?r5
[	 ]+120:[	 ]+fbe1 6e5e[ 	]+move\.d \[\$?sp=\$?sp-5\],\$?r5
[	 ]+124:[	 ]+fbe1 5e5e[ 	]+move\.w \[\$?sp=\$?sp-5\],\$?r5
[	 ]+128:[	 ]+fbe1 4e5e[ 	]+move\.b \[\$?sp=\$?sp-5\],\$?r5
[	 ]+12c:[	 ]+f8e1 6e5e[ 	]+move\.d \[\$?sp=\$?sp-8\],\$?r5
[	 ]+130:[	 ]+f8e1 5e5e[ 	]+move\.w \[\$?sp=\$?sp-8\],\$?r5
[	 ]+134:[	 ]+f8e1 4e5e[ 	]+move\.b \[\$?sp=\$?sp-8\],\$?r5
[	 ]+138:[	 ]+fce1 ee0f[ 	]+push[ ]+\$?r0
[	 ]+13c:[	 ]+6e2e[ 	]+pop[ ]+\$?r2
[	 ]+13e:[	 ]+6e3e[ 	]+pop[ ]+\$?r3
[	 ]+140:[	 ]+fce1 eedf[ 	]+push[ ]+\$?r13
