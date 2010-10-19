.text
cmp   r0,r1
cmp   r0,[r1]
cmp   r0,[r1+]
cmp   r0,#3
cmp   r0,#0x0234
cmp   r0,0x3452

cmp   r0,r1
cmp   r0,[r1]
cmp   r0,[r1+]
cmp   r0,#3
cmp   r0,#0xcdef
cmp   r0,0xcdef

cmpb  rl0,rl1
cmpb  rl0,[r1]
cmpb  rl0,[r1+]
cmpb  rl0,#3
cmpb  rl0,#cd
cmpb  rl0,0x0234

cmpb  rl0,rl1
cmpb  rl0,[r1]
cmpb  rl0,[r1+]
cmpb  rl0,#3
cmpb  rl0,#cd
cmpb  rl0,0xcdef

cmpd1	r0,#0x0f
cmpd1 	r0,#0x0fccb
cmpd1	r0,0xffcb
cmpd2	r0,#0x0f
cmpd2 	r0,#0x0fccb
cmpd2	r0,0xffcb

cmpi1	r0,#0x0f
cmpi1 	r0,#0x0fccb
cmpi1	r0,0xffcb
cmpi2	r0,#0x0f
cmpi2 	r0,#0x0fccb
cmpi2	r0,0xffcb



