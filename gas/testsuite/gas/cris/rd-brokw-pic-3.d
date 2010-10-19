#objdump: -dr
#as: --underscore --pic --em=criself
#source: brokw-3.s

.*:     file format .*-cris
Disassembly of section \.text:
0+ <start>:
[	 ]+0:[	 ]+4002[ 	]+moveq[ ]+0,\$?r0
[	 ]+2:[	 ]+af0c 5700[ 	]+subs\.b 87,\$?r0
[	 ]+6:[	 ]+cf0d 2900[ 	]+bound\.b 0x29,\$?r0
[	 ]+a:[	 ]+5f05 3ff8[ 	]+adds\.w \[\$?pc\+\$?r0\.w\],\$?pc
0+e <sym2>:
[ 	]+e:[ 	]+b401[ 	]+case 87: -> 1c2 <sym2\+0x1b4>
[ 	]+10:[ 	]+ac01[ 	]+case 88: -> 1ba <sym2\+0x1ac>
[ 	]+12:[ 	]+a401[ 	]+case 89: -> 1b2 <sym2\+0x1a4>
[ 	]+14:[ 	]+9c01[ 	]+case 90: -> 1aa <sym2\+0x19c>
[ 	]+16:[ 	]+9401[ 	]+case 91: -> 1a2 <sym2\+0x194>
[ 	]+18:[ 	]+8c01[ 	]+case 92: -> 19a <sym2\+0x18c>
[ 	]+1a:[ 	]+8401[ 	]+case 93: -> 192 <sym2\+0x184>
[ 	]+1c:[ 	]+7c01[ 	]+case 94: -> 18a <sym2\+0x17c>
[ 	]+1e:[ 	]+7401[ 	]+case 95: -> 182 <sym2\+0x174>
[ 	]+20:[ 	]+6c01[ 	]+case 96: -> 17a <sym2\+0x16c>
[ 	]+22:[ 	]+6401[ 	]+case 97: -> 172 <sym2\+0x164>
[ 	]+24:[ 	]+5c01[ 	]+case 98: -> 16a <sym2\+0x15c>
[ 	]+26:[ 	]+5401[ 	]+case 99: -> 162 <sym2\+0x154>
[ 	]+28:[ 	]+4c01[ 	]+case 100: -> 15a <sym2\+0x14c>
[ 	]+2a:[ 	]+4401[ 	]+case 101: -> 152 <sym2\+0x144>
[ 	]+2c:[ 	]+3c01[ 	]+case 102: -> 14a <sym2\+0x13c>
[ 	]+2e:[ 	]+3401[ 	]+case 103: -> 142 <sym2\+0x134>
[ 	]+30:[ 	]+2c01[ 	]+case 104: -> 13a <sym2\+0x12c>
[ 	]+32:[ 	]+2401[ 	]+case 105: -> 132 <sym2\+0x124>
[ 	]+34:[ 	]+1c01[ 	]+case 106: -> 12a <sym2\+0x11c>
[ 	]+36:[ 	]+1401[ 	]+case 107: -> 122 <sym2\+0x114>
[ 	]+38:[ 	]+0c01[ 	]+case 108: -> 11a <sym2\+0x10c>
[ 	]+3a:[ 	]+0401[ 	]+case 109: -> 112 <sym2\+0x104>
[ 	]+3c:[ 	]+fc00[ 	]+case 110: -> 10a <sym2\+0xfc>
[ 	]+3e:[ 	]+f400[ 	]+case 111: -> 102 <sym2\+0xf4>
[ 	]+40:[ 	]+ec00[ 	]+case 112: -> fa <sym2\+0xec>
[ 	]+42:[ 	]+e400[ 	]+case 113: -> f2 <sym2\+0xe4>
[ 	]+44:[ 	]+dc00[ 	]+case 114: -> ea <sym2\+0xdc>
[ 	]+46:[ 	]+d400[ 	]+case 115: -> e2 <sym2\+0xd4>
[ 	]+48:[ 	]+cc00[ 	]+case 116: -> da <sym2\+0xcc>
[ 	]+4a:[ 	]+c400[ 	]+case 117: -> d2 <sym2\+0xc4>
[ 	]+4c:[ 	]+bc00[ 	]+case 118: -> ca <sym2\+0xbc>
[ 	]+4e:[ 	]+b400[ 	]+case 119: -> c2 <sym2\+0xb4>
[ 	]+50:[ 	]+ac00[ 	]+case 120: -> ba <sym2\+0xac>
[ 	]+52:[ 	]+a400[ 	]+case 121: -> b2 <sym2\+0xa4>
[ 	]+54:[ 	]+9c00[ 	]+case 122: -> aa <sym2\+0x9c>
[ 	]+56:[ 	]+9400[ 	]+case 123: -> a2 <sym2\+0x94>
[ 	]+58:[ 	]+8c00[ 	]+case 124: -> 9a <sym2\+0x8c>
[ 	]+5a:[ 	]+8400[ 	]+case 125: -> 92 <sym2\+0x84>
[ 	]+5c:[ 	]+7c00[ 	]+case 126: -> 8a <sym2\+0x7c>
[ 	]+5e:[ 	]+7400[ 	]+case 127: -> 82 <sym2\+0x74>
[ 	]+60:[ 	]+6c00[ 	]+case 128/default: -> 7a <sym2\+0x6c>
^[ 	]+\.\.\.
[	 ]+72:[	 ]+4102[ 	]+moveq[ ]+1,\$?r0
[	 ]+74:[	 ]+ffed 5201[ 	]+ba 1ca <next_label>
[	 ]+78:[	 ]+0f05[ 	]+nop[ ]*
[ 	]+7a:[ 	]+6ffd 9e81 0000 3f0e[ 	]+move \[pc=pc\+819e <next_label\+0x7fd4>\],p0
[ 	]+82:[ 	]+6ffd 9481 0000 3f0e[ 	]+move \[pc=pc\+8194 <next_label\+0x7fca>\],p0
[ 	]+8a:[ 	]+6ffd 8a81 0000 3f0e[ 	]+move \[pc=pc\+818a <next_label\+0x7fc0>\],p0
[ 	]+92:[ 	]+6ffd 8081 0000 3f0e[ 	]+move \[pc=pc\+8180 <next_label\+0x7fb6>\],p0
[ 	]+9a:[ 	]+6ffd 7681 0000 3f0e[ 	]+move \[pc=pc\+8176 <next_label\+0x7fac>\],p0
[ 	]+a2:[ 	]+6ffd 6c81 0000 3f0e[ 	]+move \[pc=pc\+816c <next_label\+0x7fa2>\],p0
[ 	]+aa:[ 	]+6ffd 6281 0000 3f0e[ 	]+move \[pc=pc\+8162 <next_label\+0x7f98>\],p0
[ 	]+b2:[ 	]+6ffd 5881 0000 3f0e[ 	]+move \[pc=pc\+8158 <next_label\+0x7f8e>\],p0
[ 	]+ba:[ 	]+6ffd 4e81 0000 3f0e[ 	]+move \[pc=pc\+814e <next_label\+0x7f84>\],p0
[ 	]+c2:[ 	]+6ffd 4481 0000 3f0e[ 	]+move \[pc=pc\+8144 <next_label\+0x7f7a>\],p0
[ 	]+ca:[ 	]+6ffd 3a81 0000 3f0e[ 	]+move \[pc=pc\+813a <next_label\+0x7f70>\],p0
[ 	]+d2:[ 	]+6ffd 3081 0000 3f0e[ 	]+move \[pc=pc\+8130 <next_label\+0x7f66>\],p0
[ 	]+da:[ 	]+6ffd 2681 0000 3f0e[ 	]+move \[pc=pc\+8126 <next_label\+0x7f5c>\],p0
[ 	]+e2:[ 	]+6ffd 1c81 0000 3f0e[ 	]+move \[pc=pc\+811c <next_label\+0x7f52>\],p0
[ 	]+ea:[ 	]+6ffd 1281 0000 3f0e[ 	]+move \[pc=pc\+8112 <next_label\+0x7f48>\],p0
[ 	]+f2:[ 	]+6ffd 0881 0000 3f0e[ 	]+move \[pc=pc\+8108 <next_label\+0x7f3e>\],p0
[ 	]+fa:[ 	]+6ffd fe80 0000 3f0e[ 	]+move \[pc=pc\+80fe <next_label\+0x7f34>\],p0
[ 	]+102:[ 	]+6ffd f480 0000 3f0e[ 	]+move \[pc=pc\+80f4 <next_label\+0x7f2a>\],p0
[ 	]+10a:[ 	]+6ffd ea80 0000 3f0e[ 	]+move \[pc=pc\+80ea <next_label\+0x7f20>\],p0
[ 	]+112:[ 	]+6ffd e080 0000 3f0e[ 	]+move \[pc=pc\+80e0 <next_label\+0x7f16>\],p0
[ 	]+11a:[ 	]+6ffd d680 0000 3f0e[ 	]+move \[pc=pc\+80d6 <next_label\+0x7f0c>\],p0
[ 	]+122:[ 	]+6ffd cc80 0000 3f0e[ 	]+move \[pc=pc\+80cc <next_label\+0x7f02>\],p0
[ 	]+12a:[ 	]+6ffd c280 0000 3f0e[ 	]+move \[pc=pc\+80c2 <next_label\+0x7ef8>\],p0
[ 	]+132:[ 	]+6ffd b880 0000 3f0e[ 	]+move \[pc=pc\+80b8 <next_label\+0x7eee>\],p0
[ 	]+13a:[ 	]+6ffd ae80 0000 3f0e[ 	]+move \[pc=pc\+80ae <next_label\+0x7ee4>\],p0
[ 	]+142:[ 	]+6ffd a480 0000 3f0e[ 	]+move \[pc=pc\+80a4 <next_label\+0x7eda>\],p0
[ 	]+14a:[ 	]+6ffd 9a80 0000 3f0e[ 	]+move \[pc=pc\+809a <next_label\+0x7ed0>\],p0
[ 	]+152:[ 	]+6ffd 9080 0000 3f0e[ 	]+move \[pc=pc\+8090 <next_label\+0x7ec6>\],p0
[ 	]+15a:[ 	]+6ffd 8680 0000 3f0e[ 	]+move \[pc=pc\+8086 <next_label\+0x7ebc>\],p0
[ 	]+162:[ 	]+6ffd 7c80 0000 3f0e[ 	]+move \[pc=pc\+807c <next_label\+0x7eb2>\],p0
[ 	]+16a:[ 	]+6ffd 7280 0000 3f0e[ 	]+move \[pc=pc\+8072 <next_label\+0x7ea8>\],p0
[ 	]+172:[ 	]+6ffd 6880 0000 3f0e[ 	]+move \[pc=pc\+8068 <next_label\+0x7e9e>\],p0
[ 	]+17a:[ 	]+6ffd 5e80 0000 3f0e[ 	]+move \[pc=pc\+805e <next_label\+0x7e94>\],p0
[ 	]+182:[ 	]+6ffd 5480 0000 3f0e[ 	]+move \[pc=pc\+8054 <next_label\+0x7e8a>\],p0
[ 	]+18a:[ 	]+6ffd 4a80 0000 3f0e[ 	]+move \[pc=pc\+804a <next_label\+0x7e80>\],p0
[ 	]+192:[ 	]+6ffd 4080 0000 3f0e[ 	]+move \[pc=pc\+8040 <next_label\+0x7e76>\],p0
[ 	]+19a:[ 	]+6ffd 3680 0000 3f0e[ 	]+move \[pc=pc\+8036 <next_label\+0x7e6c>\],p0
[ 	]+1a2:[ 	]+6ffd 2c80 0000 3f0e[ 	]+move \[pc=pc\+802c <next_label\+0x7e62>\],p0
[ 	]+1aa:[ 	]+6ffd 2280 0000 3f0e[ 	]+move \[pc=pc\+8022 <next_label\+0x7e58>\],p0
[ 	]+1b2:[ 	]+6ffd 1880 0000 3f0e[ 	]+move \[pc=pc\+8018 <next_label\+0x7e4e>\],p0
[ 	]+1ba:[ 	]+6ffd 0e80 0000 3f0e[ 	]+move \[pc=pc\+800e <next_label\+0x7e44>\],p0
[ 	]+1c2:[ 	]+6ffd 0480 0000 3f0e[ 	]+move \[pc=pc\+8004 <next_label\+0x7e3a>\],p0
0+1ca <next_label>:
[	 ]+1ca:[	 ]+4202[ 	]+moveq[ ]+2,\$?r0
^[ 	]+\.\.\.
0+81cc <sym1>:
[ 	]+81cc:[ 	]+7d02[ 	]+moveq -3,\$?r0
0+81ce <sym3>:
[ 	]+81ce:[ 	]+4302[ 	]+moveq 3,\$?r0
0+81d0 <sym4>:
[ 	]+81d0:[ 	]+4402[ 	]+moveq 4,\$?r0
0+81d2 <sym5>:
[ 	]+81d2:[ 	]+4502[ 	]+moveq 5,\$?r0
0+81d4 <sym6>:
[ 	]+81d4:[ 	]+4602[ 	]+moveq 6,\$?r0
0+81d6 <sym7>:
[ 	]+81d6:[ 	]+4702[ 	]+moveq 7,\$?r0
0+81d8 <sym8>:
[ 	]+81d8:[ 	]+4802[ 	]+moveq 8,\$?r0
0+81da <sym9>:
[ 	]+81da:[ 	]+4902[ 	]+moveq 9,\$?r0
0+81dc <sym10>:
[ 	]+81dc:[ 	]+4a02[ 	]+moveq 10,\$?r0
0+81de <sym11>:
[ 	]+81de:[ 	]+4b02[ 	]+moveq 11,\$?r0
0+81e0 <sym12>:
[ 	]+81e0:[ 	]+4c02[ 	]+moveq 12,\$?r0
0+81e2 <sym13>:
[ 	]+81e2:[ 	]+4d02[ 	]+moveq 13,\$?r0
0+81e4 <sym14>:
[ 	]+81e4:[ 	]+4e02[ 	]+moveq 14,\$?r0
0+81e6 <sym15>:
[ 	]+81e6:[ 	]+4f02[ 	]+moveq 15,\$?r0
0+81e8 <sym16>:
[ 	]+81e8:[ 	]+5002[ 	]+moveq 16,\$?r0
0+81ea <sym17>:
[ 	]+81ea:[ 	]+5102[ 	]+moveq 17,\$?r0
0+81ec <sym18>:
[ 	]+81ec:[ 	]+5202[ 	]+moveq 18,\$?r0
0+81ee <sym19>:
[ 	]+81ee:[ 	]+5302[ 	]+moveq 19,\$?r0
0+81f0 <sym20>:
[ 	]+81f0:[ 	]+5402[ 	]+moveq 20,\$?r0
0+81f2 <sym21>:
[ 	]+81f2:[ 	]+5502[ 	]+moveq 21,\$?r0
0+81f4 <sym22>:
[ 	]+81f4:[ 	]+5602[ 	]+moveq 22,\$?r0
0+81f6 <sym23>:
[ 	]+81f6:[ 	]+5702[ 	]+moveq 23,\$?r0
0+81f8 <sym24>:
[ 	]+81f8:[ 	]+5802[ 	]+moveq 24,\$?r0
0+81fa <sym25>:
[ 	]+81fa:[ 	]+5902[ 	]+moveq 25,\$?r0
0+81fc <sym26>:
[ 	]+81fc:[ 	]+5a02[ 	]+moveq 26,\$?r0
0+81fe <sym27>:
[ 	]+81fe:[ 	]+5b02[ 	]+moveq 27,\$?r0
0+8200 <sym28>:
[ 	]+8200:[ 	]+5c02[ 	]+moveq 28,\$?r0
0+8202 <sym29>:
[ 	]+8202:[ 	]+5d02[ 	]+moveq 29,\$?r0
0+8204 <sym30>:
[ 	]+8204:[ 	]+5e02[ 	]+moveq 30,\$?r0
0+8206 <sym31>:
[ 	]+8206:[ 	]+5f02[ 	]+moveq 31,\$?r0
0+8208 <sym32>:
[ 	]+8208:[ 	]+6002[ 	]+moveq -32,\$?r0
0+820a <sym33>:
[ 	]+820a:[ 	]+6102[ 	]+moveq -31,\$?r0
0+820c <sym34>:
[ 	]+820c:[ 	]+6202[ 	]+moveq -30,\$?r0
0+820e <sym35>:
[ 	]+820e:[ 	]+6302[ 	]+moveq -29,\$?r0
0+8210 <sym36>:
[ 	]+8210:[ 	]+6402[ 	]+moveq -28,\$?r0
0+8212 <sym37>:
[ 	]+8212:[ 	]+6502[ 	]+moveq -27,\$?r0
0+8214 <sym38>:
[ 	]+8214:[ 	]+6602[ 	]+moveq -26,\$?r0
0+8216 <sym39>:
[ 	]+8216:[ 	]+6702[ 	]+moveq -25,\$?r0
0+8218 <sym40>:
[ 	]+8218:[ 	]+6802[ 	]+moveq -24,\$?r0
0+821a <sym41>:
[ 	]+821a:[ 	]+6902[ 	]+moveq -23,\$?r0
0+821c <sym42>:
[ 	]+821c:[ 	]+6a02[ 	]+moveq -22,\$?r0
0+821e <sym43>:
[ 	]+821e:[ 	]+6b02[ 	]+moveq -21,\$?r0
