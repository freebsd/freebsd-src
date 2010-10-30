        .text
        .global main
main:
	cbitb $4,0xbcd
	cbitb $5,0xaabcd
	cbitb $3,0xfaabcd

	cbitb $5,[r12]0x14
	cbitb $4,[r13]0xabfc
	cbitb $3,[r12]0x1234
	cbitb $3,[r13]0x1234
	cbitb $3,[r12]0x34

	cbitb $3,[r12]0xa7a(r1,r0)
	cbitb $3,[r12]0xa7a(r3,r2)
	cbitb $3,[r12]0xa7a(r4,r3)
	cbitb $3,[r12]0xa7a(r5,r4)
	cbitb $3,[r12]0xa7a(r6,r5)
	cbitb $3,[r12]0xa7a(r7,r6)
	cbitb $3,[r12]0xa7a(r9,r8)
	cbitb $3,[r12]0xa7a(r11,r10)
	cbitb $3,[r13]0xa7a(r1,r0)
	cbitb $3,[r13]0xa7a(r3,r2)
	cbitb $3,[r13]0xa7a(r4,r3)
	cbitb $3,[r13]0xa7a(r5,r4)
	cbitb $3,[r13]0xa7a(r6,r5)
	cbitb $3,[r13]0xa7a(r7,r6)
	cbitb $3,[r13]0xa7a(r9,r8)
	cbitb $3,[r13]0xa7a(r11,r10)
	cbitb $5,[r13]0xb7a(r4,r3)
	cbitb $1,[r12]0x17a(r6,r5)
	cbitb $1,[r13]0x134(r6,r5)
	cbitb $3,[r12]0xabcde(r4,r3)
	cbitb $5,[r13]0xabcd(r4,r3)
	cbitb $3,[r12]0xabcd(r6,r5)
	cbitb $3,[r13]0xbcde(r6,r5)

	cbitb $5,0x0(r2)
	cbitb $3,0x34(r12)
	cbitb $3,0xab(r13)
	cbitb $5,0xad(r1)
	cbitb $5,0xcd(r2)
	cbitb $5,0xfff(r0)
	cbitb $3,0xbcd(r4)
	cbitb $3,0xfff(r12)
	cbitb $3,0xfff(r13)
	cbitb $3,0xffff(r13)
	cbitb $3,0x2343(r12)
	cbitb $3,0x12345(r2)
	cbitb $3,0x4abcd(r8)
	cbitb $3,0xfabcd(r13)
	cbitb $3,0xfabcd(r8)
	cbitb $3,0xfabcd(r9)
	cbitb $3,0x4abcd(r9)

	cbitb $3,0x0(r2,r1)
	cbitb $5,0x1(r2,r1)
	cbitb $4,0x1234(r2,r1)
	cbitb $3,0x1234(r2,r1)
	cbitb $3,0x12345(r2,r1)
	cbitb $3,0x123(r2,r1)
	cbitb $3,0x12345(r2,r1)
