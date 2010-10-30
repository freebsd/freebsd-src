        .text
        .global main
main:
	tbitb $4,0xbcd
	tbitb $5,0xaabcd
	tbitb $3,0xfaabcd

	tbitb $5,[r12]0x14
	tbitb $4,[r13]0xabfc
	tbitb $3,[r12]0x1234
	tbitb $3,[r13]0x1234
	tbitb $3,[r12]0x34

	tbitb $3,[r12]0xa7a(r1,r0)
	tbitb $3,[r12]0xa7a(r3,r2)
	tbitb $3,[r12]0xa7a(r4,r3)
	tbitb $3,[r12]0xa7a(r5,r4)
	tbitb $3,[r12]0xa7a(r6,r5)
	tbitb $3,[r12]0xa7a(r7,r6)
	tbitb $3,[r12]0xa7a(r9,r8)
	tbitb $3,[r12]0xa7a(r11,r10)
	tbitb $3,[r13]0xa7a(r1,r0)
	tbitb $3,[r13]0xa7a(r3,r2)
	tbitb $3,[r13]0xa7a(r4,r3)
	tbitb $3,[r13]0xa7a(r5,r4)
	tbitb $3,[r13]0xa7a(r6,r5)
	tbitb $3,[r13]0xa7a(r7,r6)
	tbitb $3,[r13]0xa7a(r9,r8)
	tbitb $3,[r13]0xa7a(r11,r10)
	tbitb $5,[r13]0xb7a(r4,r3)
	tbitb $1,[r12]0x17a(r6,r5)
	tbitb $1,[r13]0x134(r6,r5)
	tbitb $3,[r12]0xabcde(r4,r3)
	tbitb $5,[r13]0xabcd(r4,r3)
	tbitb $3,[r12]0xabcd(r6,r5)
	tbitb $3,[r13]0xbcde(r6,r5)

	tbitb $5,0x0(r2)
	tbitb $3,0x34(r12)
	tbitb $3,0xab(r13)
	tbitb $5,0xad(r1)
	tbitb $5,0xcd(r2)
	tbitb $5,0xfff(r0)
	tbitb $3,0xbcd(r4)
	tbitb $3,0xfff(r12)
	tbitb $3,0xfff(r13)
	tbitb $3,0xffff(r13)
	tbitb $3,0x2343(r12)
	tbitb $3,0x12345(r2)
	tbitb $3,0x4abcd(r8)
	tbitb $3,0xfabcd(r13)
	tbitb $3,0xfabcd(r8)
	tbitb $3,0xfabcd(r9)
	tbitb $3,0x4abcd(r9)

	tbitb $3,0x0(r2,r1)
	tbitb $5,0x1(r2,r1)
	tbitb $4,0x1234(r2,r1)
	tbitb $3,0x1234(r2,r1)
	tbitb $3,0x12345(r2,r1)
	tbitb $3,0x123(r2,r1)
	tbitb $3,0x12345(r2,r1)
