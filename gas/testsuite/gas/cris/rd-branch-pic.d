#objdump: -dr
#as: --pic --underscore --em=criself
#source: branch.s

.*:     file format .*-cris

Disassembly of section \.text:
0+ <start_original>:
[	 ]+0:[	 ]+0f05[ 	]+nop[ ]*
0+2 <startm32>:
[	 ]+2:[	 ]+0f05[ 	]+nop[ ]*
^[ 	]+\.\.\.
0+7e6a <startm16>:
[	 ]+7e6a:[	 ]+0f05[ 	]+nop[ ]*
^[ 	]+\.\.\.
0+7f2e <start>:
[	 ]+7f2e:[	 ]+0f05[ 	]+nop[ ]*
[	 ]+7f30:[	 ]+fde0[ 	]+ba[ ]+(0x7f2e|7f2e <start>)
[	 ]+7f32:[	 ]+fb00[ 	]+bcc[ ]+(0x7f2e|7f2e <start>)
[	 ]+7f34:[	 ]+f910[ 	]+bcs[ ]+(0x7f2e|7f2e <start>)
[	 ]+7f36:[	 ]+f730[ 	]+beq[ ]+(0x7f2e|7f2e <start>)
[	 ]+7f38:[	 ]+f5f0[ 	]+bwf[ ]+(0x7f2e|7f2e <start>)
[	 ]+7f3a:[	 ]+f3f0[ 	]+bwf[ ]+(0x7f2e|7f2e <start>)
[	 ]+7f3c:[	 ]+f1f0[ 	]+bwf[ ]+(0x7f2e|7f2e <start>)
[	 ]+7f3e:[	 ]+efa0[ 	]+bge[ ]+(0x7f2e|7f2e <start>)
[	 ]+7f40:[	 ]+edc0[ 	]+bgt[ ]+(0x7f2e|7f2e <start>)
[	 ]+7f42:[	 ]+eb90[ 	]+bhi[ ]+(0x7f2e|7f2e <start>)
[	 ]+7f44:[	 ]+e900[ 	]+bcc[ ]+(0x7f2e|7f2e <start>)
[	 ]+7f46:[	 ]+e7d0[ 	]+ble[ ]+(0x7f2e|7f2e <start>)
[	 ]+7f48:[	 ]+e510[ 	]+bcs[ ]+(0x7f2e|7f2e <start>)
[	 ]+7f4a:[	 ]+e380[ 	]+bls[ ]+(0x7f2e|7f2e <start>)
[	 ]+7f4c:[	 ]+e1b0[ 	]+blt[ ]+(0x7f2e|7f2e <start>)
[	 ]+7f4e:[	 ]+df70[ 	]+bmi[ ]+(0x7f2e|7f2e <start>)
[	 ]+7f50:[	 ]+dd20[ 	]+bne[ ]+(0x7f2e|7f2e <start>)
[	 ]+7f52:[	 ]+db60[ 	]+bpl[ ]+(0x7f2e|7f2e <start>)
[	 ]+7f54:[	 ]+d940[ 	]+bvc[ ]+(0x7f2e|7f2e <start>)
[	 ]+7f56:[	 ]+d750[ 	]+bvs[ ]+(0x7f2e|7f2e <start>)
0+7f58 <start2>:
[	 ]+7f58:[	 ]+0f05[ 	]+nop[ ]*
[	 ]+7f5a:[	 ]+0fe0[ 	]+ba[ ]+(0x7e6a|7e6a <startm16>)
[	 ]+7f5c:[	 ]+0d00[ 	]+bcc[ ]+(0x7e6a|7e6a <startm16>)
[	 ]+7f5e:[	 ]+0b10[ 	]+bcs[ ]+(0x7e6a|7e6a <startm16>)
[	 ]+7f60:[	 ]+0930[ 	]+beq[ ]+(0x7e6a|7e6a <startm16>)
[	 ]+7f62:[	 ]+07f0[ 	]+bwf[ ]+(0x7e6a|7e6a <startm16>)
[	 ]+7f64:[	 ]+05f0[ 	]+bwf[ ]+(0x7e6a|7e6a <startm16>)
[	 ]+7f66:[	 ]+03f0[ 	]+bwf[ ]+(0x7e6a|7e6a <startm16>)
[	 ]+7f68:[	 ]+01a0[ 	]+bge[ ]+(0x7e6a|7e6a <startm16>)
[	 ]+7f6a:[	 ]+ffcd fcfe[ 	]+bgt (0x7e6a|7e6a <startm16>)
[	 ]+7f6e:[	 ]+ff9d f8fe[ 	]+bhi (0x7e6a|7e6a <startm16>)
[	 ]+7f72:[	 ]+ff0d f4fe[ 	]+bhs (0x7e6a|7e6a <startm16>)
[	 ]+7f76:[	 ]+ffdd f0fe[ 	]+ble (0x7e6a|7e6a <startm16>)
[	 ]+7f7a:[	 ]+ff1d ecfe[ 	]+blo (0x7e6a|7e6a <startm16>)
[	 ]+7f7e:[	 ]+ff8d e8fe[ 	]+bls (0x7e6a|7e6a <startm16>)
[	 ]+7f82:[	 ]+ffbd e4fe[ 	]+blt (0x7e6a|7e6a <startm16>)
[	 ]+7f86:[	 ]+ff7d e0fe[ 	]+bmi (0x7e6a|7e6a <startm16>)
[	 ]+7f8a:[	 ]+ff2d dcfe[ 	]+bne (0x7e6a|7e6a <startm16>)
[	 ]+7f8e:[	 ]+ff6d d8fe[ 	]+bpl (0x7e6a|7e6a <startm16>)
[	 ]+7f92:[	 ]+ff4d d4fe[ 	]+bvc (0x7e6a|7e6a <startm16>)
[	 ]+7f96:[	 ]+ff5d d0fe[ 	]+bvs (0x7e6a|7e6a <startm16>)
0+7f9a <start3>:
[	 ]+7f9a:[	 ]+0f05[ 	]+nop[ ]*
[	 ]+7f9c:[	 ]+ffed cafe[ 	]+ba (0x7e6a|7e6a <startm16>)
[	 ]+7fa0:[	 ]+ff0d c6fe[ 	]+bhs (0x7e6a|7e6a <startm16>)
[	 ]+7fa4:[	 ]+ff1d c2fe[ 	]+blo (0x7e6a|7e6a <startm16>)
[	 ]+7fa8:[	 ]+ff3d befe[ 	]+beq (0x7e6a|7e6a <startm16>)
[	 ]+7fac:[	 ]+fffd bafe[ 	]+bwf (0x7e6a|7e6a <startm16>)
[	 ]+7fb0:[	 ]+fffd b6fe[ 	]+bwf (0x7e6a|7e6a <startm16>)
[	 ]+7fb4:[	 ]+fffd b2fe[ 	]+bwf (0x7e6a|7e6a <startm16>)
[	 ]+7fb8:[	 ]+ffad aefe[ 	]+bge (0x7e6a|7e6a <startm16>)
[	 ]+7fbc:[	 ]+ffcd aafe[ 	]+bgt (0x7e6a|7e6a <startm16>)
[	 ]+7fc0:[	 ]+ff9d a6fe[ 	]+bhi (0x7e6a|7e6a <startm16>)
[	 ]+7fc4:[	 ]+ff0d a2fe[ 	]+bhs (0x7e6a|7e6a <startm16>)
[	 ]+7fc8:[	 ]+ffdd 9efe[ 	]+ble (0x7e6a|7e6a <startm16>)
[	 ]+7fcc:[	 ]+ff1d 9afe[ 	]+blo (0x7e6a|7e6a <startm16>)
[	 ]+7fd0:[	 ]+ff8d 96fe[ 	]+bls (0x7e6a|7e6a <startm16>)
[	 ]+7fd4:[	 ]+ffbd 92fe[ 	]+blt (0x7e6a|7e6a <startm16>)
[	 ]+7fd8:[	 ]+ff7d 8efe[ 	]+bmi (0x7e6a|7e6a <startm16>)
[	 ]+7fdc:[	 ]+ff2d 8afe[ 	]+bne (0x7e6a|7e6a <startm16>)
[	 ]+7fe0:[	 ]+ff6d 86fe[ 	]+bpl (0x7e6a|7e6a <startm16>)
[	 ]+7fe4:[	 ]+ff4d 82fe[ 	]+bvc (0x7e6a|7e6a <startm16>)
[	 ]+7fe8:[	 ]+ff5d 7efe[ 	]+bvs (0x7e6a|7e6a <startm16>)
0+7fec <start4>:
[	 ]+7fec:[	 ]+0f05[ 	]+nop[ ]*
[	 ]+7fee:[	 ]+ffed 1080[ 	]+ba (0x2|2 <startm32>)
[	 ]+7ff2:[	 ]+ff0d 0c80[ 	]+bhs (0x2|2 <startm32>)
[	 ]+7ff6:[	 ]+ff1d 0880[ 	]+blo (0x2|2 <startm32>)
[	 ]+7ffa:[	 ]+ff3d 0480[ 	]+beq (0x2|2 <startm32>)
[	 ]+7ffe:[	 ]+fffd 0080[ 	]+bwf (0x2|2 <startm32>)
[ 	]+8002:[ 	]+0ae0[ 	]+ba 800e <start4\+0x22>
[ 	]+8004:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+8006:[ 	]+6ffd f67f ffff 3f0e[ 	]+move \[pc=pc\+ffff7ff6 <endp32\+0xfffe7c28>\],p0
[ 	]+800e:[ 	]+f7f0[ 	]+bwf 8006 <start4\+0x1a>
[ 	]+8010:[ 	]+0ae0[ 	]+ba 801c <start4\+0x30>
[ 	]+8012:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+8014:[ 	]+6ffd e87f ffff 3f0e[ 	]+move \[pc=pc\+ffff7fe8 <endp32\+0xfffe7c1a>\],p0
[ 	]+801c:[ 	]+f7f0[ 	]+bwf 8014 <start4\+0x28>
[ 	]+801e:[ 	]+0ae0[ 	]+ba 802a <start4\+0x3e>
[ 	]+8020:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+8022:[ 	]+6ffd da7f ffff 3f0e[ 	]+move \[pc=pc\+ffff7fda <endp32\+0xfffe7c0c>\],p0
[ 	]+802a:[ 	]+f7a0[ 	]+bge 8022 <start4\+0x36>
[ 	]+802c:[ 	]+0ae0[ 	]+ba 8038 <start4\+0x4c>
[ 	]+802e:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+8030:[ 	]+6ffd cc7f ffff 3f0e[ 	]+move \[pc=pc\+ffff7fcc <endp32\+0xfffe7bfe>\],p0
[ 	]+8038:[ 	]+f7c0[ 	]+bgt 8030 <start4\+0x44>
[ 	]+803a:[ 	]+0ae0[ 	]+ba 8046 <start4\+0x5a>
[ 	]+803c:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+803e:[ 	]+6ffd be7f ffff 3f0e[ 	]+move \[pc=pc\+ffff7fbe <endp32\+0xfffe7bf0>\],p0
[ 	]+8046:[ 	]+f790[ 	]+bhi 803e <start4\+0x52>
[ 	]+8048:[ 	]+0ae0[ 	]+ba 8054 <start4\+0x68>
[ 	]+804a:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+804c:[ 	]+6ffd b07f ffff 3f0e[ 	]+move \[pc=pc\+ffff7fb0 <endp32\+0xfffe7be2>\],p0
[ 	]+8054:[ 	]+f700[ 	]+bcc 804c <start4\+0x60>
[ 	]+8056:[ 	]+0ae0[ 	]+ba 8062 <start4\+0x76>
[ 	]+8058:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+805a:[ 	]+6ffd a27f ffff 3f0e[ 	]+move \[pc=pc\+ffff7fa2 <endp32\+0xfffe7bd4>\],p0
[ 	]+8062:[ 	]+f7d0[ 	]+ble 805a <start4\+0x6e>
[ 	]+8064:[ 	]+0ae0[ 	]+ba 8070 <start4\+0x84>
[ 	]+8066:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+8068:[ 	]+6ffd 947f ffff 3f0e[ 	]+move \[pc=pc\+ffff7f94 <endp32\+0xfffe7bc6>\],p0
[ 	]+8070:[ 	]+f710[ 	]+bcs 8068 <start4\+0x7c>
[ 	]+8072:[ 	]+0ae0[ 	]+ba 807e <start4\+0x92>
[ 	]+8074:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+8076:[ 	]+6ffd 867f ffff 3f0e[ 	]+move \[pc=pc\+ffff7f86 <endp32\+0xfffe7bb8>\],p0
[ 	]+807e:[ 	]+f780[ 	]+bls 8076 <start4\+0x8a>
[ 	]+8080:[ 	]+0ae0[ 	]+ba 808c <start4\+0xa0>
[ 	]+8082:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+8084:[ 	]+6ffd 787f ffff 3f0e[ 	]+move \[pc=pc\+ffff7f78 <endp32\+0xfffe7baa>\],p0
[ 	]+808c:[ 	]+f7b0[ 	]+blt 8084 <start4\+0x98>
[ 	]+808e:[ 	]+0ae0[ 	]+ba 809a <start4\+0xae>
[ 	]+8090:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+8092:[ 	]+6ffd 6a7f ffff 3f0e[ 	]+move \[pc=pc\+ffff7f6a <endp32\+0xfffe7b9c>\],p0
[ 	]+809a:[ 	]+f770[ 	]+bmi 8092 <start4\+0xa6>
[ 	]+809c:[ 	]+0ae0[ 	]+ba 80a8 <start4\+0xbc>
[ 	]+809e:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+80a0:[ 	]+6ffd 5c7f ffff 3f0e[ 	]+move \[pc=pc\+ffff7f5c <endp32\+0xfffe7b8e>\],p0
[ 	]+80a8:[ 	]+f720[ 	]+bne 80a0 <start4\+0xb4>
[ 	]+80aa:[ 	]+0ae0[ 	]+ba 80b6 <start4\+0xca>
[ 	]+80ac:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+80ae:[ 	]+6ffd 4e7f ffff 3f0e[ 	]+move \[pc=pc\+ffff7f4e <endp32\+0xfffe7b80>\],p0
[ 	]+80b6:[ 	]+f760[ 	]+bpl 80ae <start4\+0xc2>
[ 	]+80b8:[ 	]+0ae0[ 	]+ba 80c4 <start4\+0xd8>
[ 	]+80ba:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+80bc:[ 	]+6ffd 407f ffff 3f0e[ 	]+move \[pc=pc\+ffff7f40 <endp32\+0xfffe7b72>\],p0
[ 	]+80c4:[ 	]+f740[ 	]+bvc 80bc <start4\+0xd0>
[ 	]+80c6:[ 	]+0ae0[ 	]+ba 80d2 <start4\+0xe6>
[ 	]+80c8:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+80ca:[ 	]+6ffd 327f ffff 3f0e[ 	]+move \[pc=pc\+ffff7f32 <endp32\+0xfffe7b64>\],p0
[ 	]+80d2:[ 	]+f750[ 	]+bvs 80ca <start4\+0xde>
0+80d4 <start5>:
[ 	]+80d4:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+80d6:[ 	]+0ae0[ 	]+ba 80e2 <start5\+0xe>
[ 	]+80d8:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+80da:[ 	]+6ffd 227f ffff 3f0e[ 	]+move \[pc=pc\+ffff7f22 <endp32\+0xfffe7b54>\],p0
[ 	]+80e2:[ 	]+f7e0[ 	]+ba 80da <start5\+0x6>
[ 	]+80e4:[ 	]+0ae0[ 	]+ba 80f0 <start5\+0x1c>
[ 	]+80e6:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+80e8:[ 	]+6ffd 147f ffff 3f0e[ 	]+move \[pc=pc\+ffff7f14 <endp32\+0xfffe7b46>\],p0
[ 	]+80f0:[ 	]+f700[ 	]+bcc 80e8 <start5\+0x14>
[ 	]+80f2:[ 	]+0ae0[ 	]+ba 80fe <start5\+0x2a>
[ 	]+80f4:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+80f6:[ 	]+6ffd 067f ffff 3f0e[ 	]+move \[pc=pc\+ffff7f06 <endp32\+0xfffe7b38>\],p0
[ 	]+80fe:[ 	]+f710[ 	]+bcs 80f6 <start5\+0x22>
[ 	]+8100:[ 	]+0ae0[ 	]+ba 810c <start5\+0x38>
[ 	]+8102:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+8104:[ 	]+6ffd f87e ffff 3f0e[ 	]+move \[pc=pc\+ffff7ef8 <endp32\+0xfffe7b2a>\],p0
[ 	]+810c:[ 	]+f730[ 	]+beq 8104 <start5\+0x30>
[ 	]+810e:[ 	]+0ae0[ 	]+ba 811a <start5\+0x46>
[ 	]+8110:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+8112:[ 	]+6ffd ea7e ffff 3f0e[ 	]+move \[pc=pc\+ffff7eea <endp32\+0xfffe7b1c>\],p0
[ 	]+811a:[ 	]+f7f0[ 	]+bwf 8112 <start5\+0x3e>
[ 	]+811c:[ 	]+0ae0[ 	]+ba 8128 <start5\+0x54>
[ 	]+811e:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+8120:[ 	]+6ffd dc7e ffff 3f0e[ 	]+move \[pc=pc\+ffff7edc <endp32\+0xfffe7b0e>\],p0
[ 	]+8128:[ 	]+f7f0[ 	]+bwf 8120 <start5\+0x4c>
[ 	]+812a:[ 	]+0ae0[ 	]+ba 8136 <start5\+0x62>
[ 	]+812c:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+812e:[ 	]+6ffd ce7e ffff 3f0e[ 	]+move \[pc=pc\+ffff7ece <endp32\+0xfffe7b00>\],p0
[ 	]+8136:[ 	]+f7f0[ 	]+bwf 812e <start5\+0x5a>
[ 	]+8138:[ 	]+0ae0[ 	]+ba 8144 <start5\+0x70>
[ 	]+813a:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+813c:[ 	]+6ffd c07e ffff 3f0e[ 	]+move \[pc=pc\+ffff7ec0 <endp32\+0xfffe7af2>\],p0
[ 	]+8144:[ 	]+f7a0[ 	]+bge 813c <start5\+0x68>
[ 	]+8146:[ 	]+0ae0[ 	]+ba 8152 <start5\+0x7e>
[ 	]+8148:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+814a:[ 	]+6ffd b27e ffff 3f0e[ 	]+move \[pc=pc\+ffff7eb2 <endp32\+0xfffe7ae4>\],p0
[ 	]+8152:[ 	]+f7c0[ 	]+bgt 814a <start5\+0x76>
[ 	]+8154:[ 	]+0ae0[ 	]+ba 8160 <start5\+0x8c>
[ 	]+8156:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+8158:[ 	]+6ffd a47e ffff 3f0e[ 	]+move \[pc=pc\+ffff7ea4 <endp32\+0xfffe7ad6>\],p0
[ 	]+8160:[ 	]+f790[ 	]+bhi 8158 <start5\+0x84>
[ 	]+8162:[ 	]+0ae0[ 	]+ba 816e <start5\+0x9a>
[ 	]+8164:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+8166:[ 	]+6ffd 967e ffff 3f0e[ 	]+move \[pc=pc\+ffff7e96 <endp32\+0xfffe7ac8>\],p0
[ 	]+816e:[ 	]+f700[ 	]+bcc 8166 <start5\+0x92>
[ 	]+8170:[ 	]+0ae0[ 	]+ba 817c <start5\+0xa8>
[ 	]+8172:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+8174:[ 	]+6ffd 887e ffff 3f0e[ 	]+move \[pc=pc\+ffff7e88 <endp32\+0xfffe7aba>\],p0
[ 	]+817c:[ 	]+f7d0[ 	]+ble 8174 <start5\+0xa0>
[ 	]+817e:[ 	]+0ae0[ 	]+ba 818a <start5\+0xb6>
[ 	]+8180:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+8182:[ 	]+6ffd 7a7e ffff 3f0e[ 	]+move \[pc=pc\+ffff7e7a <endp32\+0xfffe7aac>\],p0
[ 	]+818a:[ 	]+f710[ 	]+bcs 8182 <start5\+0xae>
[ 	]+818c:[ 	]+0ae0[ 	]+ba 8198 <start5\+0xc4>
[ 	]+818e:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+8190:[ 	]+6ffd 6c7e ffff 3f0e[ 	]+move \[pc=pc\+ffff7e6c <endp32\+0xfffe7a9e>\],p0
[ 	]+8198:[ 	]+f780[ 	]+bls 8190 <start5\+0xbc>
[ 	]+819a:[ 	]+0ae0[ 	]+ba 81a6 <start5\+0xd2>
[ 	]+819c:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+819e:[ 	]+6ffd 5e7e ffff 3f0e[ 	]+move \[pc=pc\+ffff7e5e <endp32\+0xfffe7a90>\],p0
[ 	]+81a6:[ 	]+f7b0[ 	]+blt 819e <start5\+0xca>
[ 	]+81a8:[ 	]+0ae0[ 	]+ba 81b4 <start5\+0xe0>
[ 	]+81aa:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+81ac:[ 	]+6ffd 507e ffff 3f0e[ 	]+move \[pc=pc\+ffff7e50 <endp32\+0xfffe7a82>\],p0
[ 	]+81b4:[ 	]+f770[ 	]+bmi 81ac <start5\+0xd8>
[ 	]+81b6:[ 	]+0ae0[ 	]+ba 81c2 <start5\+0xee>
[ 	]+81b8:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+81ba:[ 	]+6ffd 427e ffff 3f0e[ 	]+move \[pc=pc\+ffff7e42 <endp32\+0xfffe7a74>\],p0
[ 	]+81c2:[ 	]+f720[ 	]+bne 81ba <start5\+0xe6>
[ 	]+81c4:[ 	]+0ae0[ 	]+ba 81d0 <start5\+0xfc>
[ 	]+81c6:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+81c8:[ 	]+6ffd 347e ffff 3f0e[ 	]+move \[pc=pc\+ffff7e34 <endp32\+0xfffe7a66>\],p0
[ 	]+81d0:[ 	]+f760[ 	]+bpl 81c8 <start5\+0xf4>
[ 	]+81d2:[ 	]+0ae0[ 	]+ba 81de <start5\+0x10a>
[ 	]+81d4:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+81d6:[ 	]+6ffd 267e ffff 3f0e[ 	]+move \[pc=pc\+ffff7e26 <endp32\+0xfffe7a58>\],p0
[ 	]+81de:[ 	]+f740[ 	]+bvc 81d6 <start5\+0x102>
[ 	]+81e0:[ 	]+0ae0[ 	]+ba 81ec <start5\+0x118>
[ 	]+81e2:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+81e4:[ 	]+6ffd 187e ffff 3f0e[ 	]+move \[pc=pc\+ffff7e18 <endp32\+0xfffe7a4a>\],p0
[ 	]+81ec:[ 	]+f750[ 	]+bvs 81e4 <start5\+0x110>
0+81ee <start6>:
[ 	]+81ee:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+81f0:[ 	]+0ae0[ 	]+ba 81fc <start6\+0xe>
[ 	]+81f2:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+81f4:[ 	]+6ffd d481 0000 3f0e[ 	]+move \[pc=pc\+81d4 <start5\+0x100>\],p0
[ 	]+81fc:[ 	]+f7e0[ 	]+ba 81f4 <start6\+0x6>
[ 	]+81fe:[ 	]+0ae0[ 	]+ba 820a <start6\+0x1c>
[ 	]+8200:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+8202:[ 	]+6ffd c681 0000 3f0e[ 	]+move \[pc=pc\+81c6 <start5\+0xf2>\],p0
[ 	]+820a:[ 	]+f700[ 	]+bcc 8202 <start6\+0x14>
[ 	]+820c:[ 	]+0ae0[ 	]+ba 8218 <start6\+0x2a>
[ 	]+820e:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+8210:[ 	]+6ffd b881 0000 3f0e[ 	]+move \[pc=pc\+81b8 <start5\+0xe4>\],p0
[ 	]+8218:[ 	]+f710[ 	]+bcs 8210 <start6\+0x22>
[ 	]+821a:[ 	]+0ae0[ 	]+ba 8226 <start6\+0x38>
[ 	]+821c:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+821e:[ 	]+6ffd aa81 0000 3f0e[ 	]+move \[pc=pc\+81aa <start5\+0xd6>\],p0
[ 	]+8226:[ 	]+f730[ 	]+beq 821e <start6\+0x30>
[ 	]+8228:[ 	]+0ae0[ 	]+ba 8234 <start6\+0x46>
[ 	]+822a:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+822c:[ 	]+6ffd 9c81 0000 3f0e[ 	]+move \[pc=pc\+819c <start5\+0xc8>\],p0
[ 	]+8234:[ 	]+f7f0[ 	]+bwf 822c <start6\+0x3e>
[ 	]+8236:[ 	]+0ae0[ 	]+ba 8242 <start6\+0x54>
[ 	]+8238:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+823a:[ 	]+6ffd 8e81 0000 3f0e[ 	]+move \[pc=pc\+818e <start5\+0xba>\],p0
[ 	]+8242:[ 	]+f7f0[ 	]+bwf 823a <start6\+0x4c>
[ 	]+8244:[ 	]+0ae0[ 	]+ba 8250 <start6\+0x62>
[ 	]+8246:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+8248:[ 	]+6ffd 8081 0000 3f0e[ 	]+move \[pc=pc\+8180 <start5\+0xac>\],p0
[ 	]+8250:[ 	]+f7f0[ 	]+bwf 8248 <start6\+0x5a>
[ 	]+8252:[ 	]+0ae0[ 	]+ba 825e <start6\+0x70>
[ 	]+8254:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+8256:[ 	]+6ffd 7281 0000 3f0e[ 	]+move \[pc=pc\+8172 <start5\+0x9e>\],p0
[ 	]+825e:[ 	]+f7a0[ 	]+bge 8256 <start6\+0x68>
[ 	]+8260:[ 	]+0ae0[ 	]+ba 826c <start6\+0x7e>
[ 	]+8262:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+8264:[ 	]+6ffd 6481 0000 3f0e[ 	]+move \[pc=pc\+8164 <start5\+0x90>\],p0
[ 	]+826c:[ 	]+f7c0[ 	]+bgt 8264 <start6\+0x76>
[ 	]+826e:[ 	]+0ae0[ 	]+ba 827a <start6\+0x8c>
[ 	]+8270:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+8272:[ 	]+6ffd 5681 0000 3f0e[ 	]+move \[pc=pc\+8156 <start5\+0x82>\],p0
[ 	]+827a:[ 	]+f790[ 	]+bhi 8272 <start6\+0x84>
[ 	]+827c:[ 	]+0ae0[ 	]+ba 8288 <start6\+0x9a>
[ 	]+827e:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+8280:[ 	]+6ffd 4881 0000 3f0e[ 	]+move \[pc=pc\+8148 <start5\+0x74>\],p0
[ 	]+8288:[ 	]+f700[ 	]+bcc 8280 <start6\+0x92>
[ 	]+828a:[ 	]+0ae0[ 	]+ba 8296 <start6\+0xa8>
[ 	]+828c:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+828e:[ 	]+6ffd 3a81 0000 3f0e[ 	]+move \[pc=pc\+813a <start5\+0x66>\],p0
[ 	]+8296:[ 	]+f7d0[ 	]+ble 828e <start6\+0xa0>
[ 	]+8298:[ 	]+0ae0[ 	]+ba 82a4 <start6\+0xb6>
[ 	]+829a:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+829c:[ 	]+6ffd 2c81 0000 3f0e[ 	]+move \[pc=pc\+812c <start5\+0x58>\],p0
[ 	]+82a4:[ 	]+f710[ 	]+bcs 829c <start6\+0xae>
[ 	]+82a6:[ 	]+0ae0[ 	]+ba 82b2 <start6\+0xc4>
[ 	]+82a8:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+82aa:[ 	]+6ffd 1e81 0000 3f0e[ 	]+move \[pc=pc\+811e <start5\+0x4a>\],p0
[ 	]+82b2:[ 	]+f780[ 	]+bls 82aa <start6\+0xbc>
[ 	]+82b4:[ 	]+0ae0[ 	]+ba 82c0 <start6\+0xd2>
[ 	]+82b6:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+82b8:[ 	]+6ffd 1081 0000 3f0e[ 	]+move \[pc=pc\+8110 <start5\+0x3c>\],p0
[ 	]+82c0:[ 	]+f7b0[ 	]+blt 82b8 <start6\+0xca>
[ 	]+82c2:[ 	]+0ae0[ 	]+ba 82ce <start6\+0xe0>
[ 	]+82c4:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+82c6:[ 	]+6ffd 0281 0000 3f0e[ 	]+move \[pc=pc\+8102 <start5\+0x2e>\],p0
[ 	]+82ce:[ 	]+f770[ 	]+bmi 82c6 <start6\+0xd8>
[ 	]+82d0:[ 	]+0ae0[ 	]+ba 82dc <start6\+0xee>
[ 	]+82d2:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+82d4:[ 	]+6ffd f480 0000 3f0e[ 	]+move \[pc=pc\+80f4 <start5\+0x20>\],p0
[ 	]+82dc:[ 	]+f720[ 	]+bne 82d4 <start6\+0xe6>
[ 	]+82de:[ 	]+0ae0[ 	]+ba 82ea <start6\+0xfc>
[ 	]+82e0:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+82e2:[ 	]+6ffd e680 0000 3f0e[ 	]+move \[pc=pc\+80e6 <start5\+0x12>\],p0
[ 	]+82ea:[ 	]+f760[ 	]+bpl 82e2 <start6\+0xf4>
[ 	]+82ec:[ 	]+0ae0[ 	]+ba 82f8 <start6\+0x10a>
[ 	]+82ee:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+82f0:[ 	]+6ffd d880 0000 3f0e[ 	]+move \[pc=pc\+80d8 <start5\+0x4>\],p0
[ 	]+82f8:[ 	]+f740[ 	]+bvc 82f0 <start6\+0x102>
[ 	]+82fa:[ 	]+0ae0[ 	]+ba 8306 <start6\+0x118>
[ 	]+82fc:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+82fe:[ 	]+6ffd ca80 0000 3f0e[ 	]+move \[pc=pc\+80ca <start4\+0xde>\],p0
[ 	]+8306:[ 	]+f750[ 	]+bvs 82fe <start6\+0x110>
0+8308 <start7>:
[ 	]+8308:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+830a:[ 	]+0ae0[ 	]+ba 8316 <start7\+0xe>
[ 	]+830c:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+830e:[ 	]+6ffd ba80 0000 3f0e[ 	]+move \[pc=pc\+80ba <start4\+0xce>\],p0
[ 	]+8316:[ 	]+f7e0[ 	]+ba 830e <start7\+0x6>
[ 	]+8318:[ 	]+0ae0[ 	]+ba 8324 <start7\+0x1c>
[ 	]+831a:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+831c:[ 	]+6ffd ac80 0000 3f0e[ 	]+move \[pc=pc\+80ac <start4\+0xc0>\],p0
[ 	]+8324:[ 	]+f700[ 	]+bcc 831c <start7\+0x14>
[ 	]+8326:[ 	]+0ae0[ 	]+ba 8332 <start7\+0x2a>
[ 	]+8328:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+832a:[ 	]+6ffd 9e80 0000 3f0e[ 	]+move \[pc=pc\+809e <start4\+0xb2>\],p0
[ 	]+8332:[ 	]+f710[ 	]+bcs 832a <start7\+0x22>
[ 	]+8334:[ 	]+0ae0[ 	]+ba 8340 <start7\+0x38>
[ 	]+8336:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+8338:[ 	]+6ffd 9080 0000 3f0e[ 	]+move \[pc=pc\+8090 <start4\+0xa4>\],p0
[ 	]+8340:[ 	]+f730[ 	]+beq 8338 <start7\+0x30>
[ 	]+8342:[ 	]+0ae0[ 	]+ba 834e <start7\+0x46>
[ 	]+8344:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+8346:[ 	]+6ffd 8280 0000 3f0e[ 	]+move \[pc=pc\+8082 <start4\+0x96>\],p0
[ 	]+834e:[ 	]+f7f0[ 	]+bwf 8346 <start7\+0x3e>
[ 	]+8350:[ 	]+0ae0[ 	]+ba 835c <start7\+0x54>
[ 	]+8352:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+8354:[ 	]+6ffd 7480 0000 3f0e[ 	]+move \[pc=pc\+8074 <start4\+0x88>\],p0
[ 	]+835c:[ 	]+f7f0[ 	]+bwf 8354 <start7\+0x4c>
[ 	]+835e:[ 	]+0ae0[ 	]+ba 836a <start7\+0x62>
[ 	]+8360:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+8362:[ 	]+6ffd 6680 0000 3f0e[ 	]+move \[pc=pc\+8066 <start4\+0x7a>\],p0
[ 	]+836a:[ 	]+f7f0[ 	]+bwf 8362 <start7\+0x5a>
[ 	]+836c:[ 	]+0ae0[ 	]+ba 8378 <start7\+0x70>
[ 	]+836e:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+8370:[ 	]+6ffd 5880 0000 3f0e[ 	]+move \[pc=pc\+8058 <start4\+0x6c>\],p0
[ 	]+8378:[ 	]+f7a0[ 	]+bge 8370 <start7\+0x68>
[ 	]+837a:[ 	]+0ae0[ 	]+ba 8386 <start7\+0x7e>
[ 	]+837c:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+837e:[ 	]+6ffd 4a80 0000 3f0e[ 	]+move \[pc=pc\+804a <start4\+0x5e>\],p0
[ 	]+8386:[ 	]+f7c0[ 	]+bgt 837e <start7\+0x76>
[ 	]+8388:[ 	]+0ae0[ 	]+ba 8394 <start7\+0x8c>
[ 	]+838a:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+838c:[ 	]+6ffd 3c80 0000 3f0e[ 	]+move \[pc=pc\+803c <start4\+0x50>\],p0
[ 	]+8394:[ 	]+f790[ 	]+bhi 838c <start7\+0x84>
[ 	]+8396:[ 	]+0ae0[ 	]+ba 83a2 <start7\+0x9a>
[ 	]+8398:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+839a:[ 	]+6ffd 2e80 0000 3f0e[ 	]+move \[pc=pc\+802e <start4\+0x42>\],p0
[ 	]+83a2:[ 	]+f700[ 	]+bcc 839a <start7\+0x92>
[ 	]+83a4:[ 	]+0ae0[ 	]+ba 83b0 <start7\+0xa8>
[ 	]+83a6:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+83a8:[ 	]+6ffd 2080 0000 3f0e[ 	]+move \[pc=pc\+8020 <start4\+0x34>\],p0
[ 	]+83b0:[ 	]+f7d0[ 	]+ble 83a8 <start7\+0xa0>
[ 	]+83b2:[ 	]+0ae0[ 	]+ba 83be <start7\+0xb6>
[ 	]+83b4:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+83b6:[ 	]+6ffd 1280 0000 3f0e[ 	]+move \[pc=pc\+8012 <start4\+0x26>\],p0
[ 	]+83be:[ 	]+f710[ 	]+bcs 83b6 <start7\+0xae>
[ 	]+83c0:[ 	]+0ae0[ 	]+ba 83cc <start7\+0xc4>
[ 	]+83c2:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+83c4:[ 	]+6ffd 0480 0000 3f0e[ 	]+move \[pc=pc\+8004 <start4\+0x18>\],p0
[ 	]+83cc:[ 	]+f780[ 	]+bls 83c4 <start7\+0xbc>
[ 	]+83ce:[ 	]+ffbd fc7f[ 	]+blt 103ce <endp32>
[ 	]+83d2:[ 	]+ff7d f87f[ 	]+bmi 103ce <endp32>
[ 	]+83d6:[ 	]+ff2d f47f[ 	]+bne 103ce <endp32>
[ 	]+83da:[ 	]+ff6d f07f[ 	]+bpl 103ce <endp32>
[ 	]+83de:[ 	]+ff4d ec7f[ 	]+bvc 103ce <endp32>
[ 	]+83e2:[ 	]+ff5d e87f[ 	]+bvs 103ce <endp32>
0+83e6 <start8>:
[ 	]+83e6:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+83e8:[ 	]+ffed 7a01[ 	]+ba 8566 <endp16>
[ 	]+83ec:[ 	]+ff0d 7601[ 	]+bhs 8566 <endp16>
[ 	]+83f0:[ 	]+ff1d 7201[ 	]+blo 8566 <endp16>
[ 	]+83f4:[ 	]+ff3d 6e01[ 	]+beq 8566 <endp16>
[ 	]+83f8:[ 	]+fffd 6a01[ 	]+bwf 8566 <endp16>
[ 	]+83fc:[ 	]+fffd 6601[ 	]+bwf 8566 <endp16>
[ 	]+8400:[ 	]+fffd 6201[ 	]+bwf 8566 <endp16>
[ 	]+8404:[ 	]+ffad 5e01[ 	]+bge 8566 <endp16>
[ 	]+8408:[ 	]+ffcd 5a01[ 	]+bgt 8566 <endp16>
[ 	]+840c:[ 	]+ff9d 5601[ 	]+bhi 8566 <endp16>
[ 	]+8410:[ 	]+ff0d 5201[ 	]+bhs 8566 <endp16>
[ 	]+8414:[ 	]+ffdd 4e01[ 	]+ble 8566 <endp16>
[ 	]+8418:[ 	]+ff1d 4a01[ 	]+blo 8566 <endp16>
[ 	]+841c:[ 	]+ff8d 4601[ 	]+bls 8566 <endp16>
[ 	]+8420:[ 	]+ffbd 4201[ 	]+blt 8566 <endp16>
[ 	]+8424:[ 	]+ff7d 3e01[ 	]+bmi 8566 <endp16>
[ 	]+8428:[ 	]+ff2d 3a01[ 	]+bne 8566 <endp16>
[ 	]+842c:[ 	]+ff6d 3601[ 	]+bpl 8566 <endp16>
[ 	]+8430:[ 	]+ff4d 3201[ 	]+bvc 8566 <endp16>
[ 	]+8434:[ 	]+ff5d 2e01[ 	]+bvs 8566 <endp16>
0+8438 <start9>:
[ 	]+8438:[ 	]+0f05[ 	]+nop[ 	]*
[ 	]+843a:[ 	]+ffed 2801[ 	]+ba 8566 <endp16>
[ 	]+843e:[ 	]+ff0d 2401[ 	]+bhs 8566 <endp16>
[ 	]+8442:[ 	]+ff1d 2001[ 	]+blo 8566 <endp16>
[ 	]+8446:[ 	]+ff3d 1c01[ 	]+beq 8566 <endp16>
[ 	]+844a:[ 	]+fffd 1801[ 	]+bwf 8566 <endp16>
[ 	]+844e:[ 	]+fffd 1401[ 	]+bwf 8566 <endp16>
[ 	]+8452:[ 	]+fffd 1001[ 	]+bwf 8566 <endp16>
[ 	]+8456:[ 	]+ffad 0c01[ 	]+bge 8566 <endp16>
[ 	]+845a:[ 	]+ffcd 0801[ 	]+bgt 8566 <endp16>
[ 	]+845e:[ 	]+ff9d 0401[ 	]+bhi 8566 <endp16>
[ 	]+8462:[ 	]+ff0d 0001[ 	]+bhs 8566 <endp16>
[ 	]+8466:[ 	]+fed0[ 	]+ble 8566 <endp16>
[ 	]+8468:[ 	]+fc10[ 	]+bcs 8566 <endp16>
[ 	]+846a:[ 	]+fa80[ 	]+bls 8566 <endp16>
[ 	]+846c:[ 	]+f8b0[ 	]+blt 8566 <endp16>
[ 	]+846e:[ 	]+f670[ 	]+bmi 8566 <endp16>
[ 	]+8470:[ 	]+f420[ 	]+bne 8566 <endp16>
[ 	]+8472:[ 	]+f260[ 	]+bpl 8566 <endp16>
[ 	]+8474:[ 	]+f040[ 	]+bvc 8566 <endp16>
[ 	]+8476:[ 	]+ee50[ 	]+bvs 8566 <endp16>
0+8478 <start10>:
[ 	]+8478:[ 	]+28e0[ 	]+ba 84a2 <end>
[ 	]+847a:[ 	]+2600[ 	]+bcc 84a2 <end>
[ 	]+847c:[ 	]+2410[ 	]+bcs 84a2 <end>
[ 	]+847e:[ 	]+2230[ 	]+beq 84a2 <end>
[ 	]+8480:[ 	]+20f0[ 	]+bwf 84a2 <end>
[ 	]+8482:[ 	]+1ef0[ 	]+bwf 84a2 <end>
[ 	]+8484:[ 	]+1cf0[ 	]+bwf 84a2 <end>
[ 	]+8486:[ 	]+1aa0[ 	]+bge 84a2 <end>
[ 	]+8488:[ 	]+18c0[ 	]+bgt 84a2 <end>
[ 	]+848a:[ 	]+1690[ 	]+bhi 84a2 <end>
[ 	]+848c:[ 	]+1400[ 	]+bcc 84a2 <end>
[ 	]+848e:[ 	]+12d0[ 	]+ble 84a2 <end>
[ 	]+8490:[ 	]+1010[ 	]+bcs 84a2 <end>
[ 	]+8492:[ 	]+0e80[ 	]+bls 84a2 <end>
[ 	]+8494:[ 	]+0cb0[ 	]+blt 84a2 <end>
[ 	]+8496:[ 	]+0a70[ 	]+bmi 84a2 <end>
[ 	]+8498:[ 	]+0820[ 	]+bne 84a2 <end>
[ 	]+849a:[ 	]+0660[ 	]+bpl 84a2 <end>
[ 	]+849c:[ 	]+0440[ 	]+bvc 84a2 <end>
[ 	]+849e:[ 	]+0250[ 	]+bvs 84a2 <end>
[ 	]+84a0:[ 	]+0f05[ 	]+nop[ ]*
0+84a2 <end>:
[	 ]+84a2:[	 ]+0f05[ 	]+nop[ ]*
^[ 	]+\.\.\.
0+8566 <endp16>:
[	 ]+8566:[	 ]+0f05[ 	]+nop[ ]*
^[ 	]+\.\.\.
0+103ce <endp32>:
[	 ]+103ce:[	 ]+0f05[ 	]+nop[ ]*
