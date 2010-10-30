        .text
        .global main
main:
bal     (ra),*+0xff122
bal     (ra),*+0xfff126
bal     (ra),*+0x22
bal     (ra),*+0x122
bal     (ra),*+0xf122
bal     (ra),*+0x812a
bal	(r1,r0),*+0x122
bal	(r11,r10),*+0xcff122
bal	(r7,r6),*+0xaff122
bal	(r4,r3),*+0x8ff122
bal	(r8,r7),*+0xfff122
