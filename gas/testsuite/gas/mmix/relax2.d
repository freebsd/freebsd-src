#objdump: -r
#as: -x

.*:     file format elf64-mmix
R.* \[\.text\.a0\]:
O.*
0+40004 R_MMIX_PUSHJ      \.text\.a0
0+40018 R_MMIX_PUSHJ      \.text\.a0\+0x0+4
R.* \[\.text\.b0\]:
O.*
0+40004 R_MMIX_PUSHJ      \.text\.b0
0+40018 R_MMIX_PUSHJ      \.text\.b0\+0x0+4
0+4002c R_MMIX_PUSHJ      \.text\.b0\+0x0+8
R.* \[\.text\.c0\]:
O.*
0+ R_MMIX_PUSHJ      ca0
0+14 R_MMIX_PUSHJ      cb0
R.* \[\.text\.d0\]:
O.*
0+ R_MMIX_PUSHJ      da0
0+14 R_MMIX_PUSHJ      db0
0+28 R_MMIX_PUSHJ      dc0
R.* \[\.text\.a1\]:
O.*
0+40004 R_MMIX_PUSHJ      \.text\.a1
0+40018 R_MMIX_PUSHJ_STUBBABLE  \.text\.a1\+0x0+4
R.* \[\.text\.b1\]:
O.*
0+40004 R_MMIX_PUSHJ      \.text\.b1
0+40018 R_MMIX_PUSHJ      \.text\.b1\+0x0+4
0+4002c R_MMIX_PUSHJ_STUBBABLE  \.text\.b1\+0x0+8
R.* \[\.text\.c1\]:
O.*
0+ R_MMIX_PUSHJ      ca1
0+14 R_MMIX_PUSHJ_STUBBABLE  cb1
R.* \[\.text\.d1\]:
O.*
0+ R_MMIX_PUSHJ      da1
0+14 R_MMIX_PUSHJ      db1
0+28 R_MMIX_PUSHJ_STUBBABLE  dc1

# The following shows a limitation of the PUSHJ relaxation code when
# PUSHJ:s are close, and about 256k away from the section limit: On the
# first relaxation iteration, the first (or second) PUSHJ looks like it
# could reach a stub.  However, the last PUSHJ is expanded and on the
# second iteration, the stubbed PUSHJ has to be expanded too because it
# can't reach the stubs anymore.  This continues for the next iterations,
# because the max stub size is five tetrabytes (4-bytes).  At the expense
# of much more complex relaxation code (including the relaxation machinery
# in write.c), this is fixable.  Anyway, as long as PUSHJ:s aren't closer
# than five instructions, the existing code does suffice; we're just here
# to check that the border case *works* and doesn't generate invalid code.

R.* \[\.text\.a2\]:
O.*
0+40004 R_MMIX_PUSHJ      \.text\.a2
0+40018 R_MMIX_PUSHJ      \.text\.a2\+0x0+4
R.* \[\.text\.b2\]:
O.*
0+40004 R_MMIX_PUSHJ      \.text\.b2
0+40018 R_MMIX_PUSHJ      \.text\.b2\+0x0+4
0+4002c R_MMIX_PUSHJ      \.text\.b2\+0x0+8
R.* \[\.text\.c2\]:
O.*
0+ R_MMIX_PUSHJ      ca2
0+14 R_MMIX_PUSHJ      cb2
R.* \[\.text\.d2\]:
O.*
0+ R_MMIX_PUSHJ      da2
0+14 R_MMIX_PUSHJ      db2
0+28 R_MMIX_PUSHJ      dc2
R.* \[\.text\.a3\]:
O.*
0+40004 R_MMIX_PUSHJ      \.text\.a3
0+40018 R_MMIX_PUSHJ      \.text\.a3\+0x0+4
R.* \[\.text\.b3\]:
O.*
0+40004 R_MMIX_PUSHJ      \.text\.b3
0+40018 R_MMIX_PUSHJ      \.text\.b3\+0x0+4
0+4002c R_MMIX_PUSHJ      \.text\.b3\+0x0+8
R.* \[\.text\.c3\]:
O.*
0+ R_MMIX_PUSHJ      ca3
0+14 R_MMIX_PUSHJ      cb3
R.* \[\.text\.d3\]:
O.*
0+ R_MMIX_PUSHJ      da3
0+14 R_MMIX_PUSHJ      db3
0+28 R_MMIX_PUSHJ      dc3
R.* \[\.text\.a4\]:
O.*
0+40004 R_MMIX_PUSHJ      \.text\.a4
0+40018 R_MMIX_PUSHJ      \.text\.a4\+0x0+4
R.* \[\.text\.b4\]:
O.*
0+40004 R_MMIX_PUSHJ      \.text\.b4
0+40018 R_MMIX_PUSHJ      \.text\.b4\+0x0+4
0+4002c R_MMIX_PUSHJ      \.text\.b4\+0x0+8
R.* \[\.text\.c4\]:
O.*
0+ R_MMIX_PUSHJ      ca4
0+14 R_MMIX_PUSHJ      cb4
R.* \[\.text\.d4\]:
O.*
0+ R_MMIX_PUSHJ      da4
0+14 R_MMIX_PUSHJ      db4
0+28 R_MMIX_PUSHJ      dc4
R.* \[\.text\.a5\]:
O.*
0+40004 R_MMIX_PUSHJ      \.text\.a5
0+40018 R_MMIX_PUSHJ      \.text\.a5\+0x0+4
R.* \[\.text\.b5\]:
O.*
0+40004 R_MMIX_PUSHJ      \.text\.b5
0+40018 R_MMIX_PUSHJ      \.text\.b5\+0x0+4
0+4002c R_MMIX_PUSHJ      \.text\.b5\+0x0+8
R.* \[\.text\.c5\]:
O.*
0+ R_MMIX_PUSHJ      ca5
0+14 R_MMIX_PUSHJ      cb5
R.* \[\.text\.d5\]:
O.*
0+ R_MMIX_PUSHJ      da5
0+14 R_MMIX_PUSHJ      db5
0+28 R_MMIX_PUSHJ      dc5
R.* \[\.text\.a6\]:
O.*
0+40004 R_MMIX_PUSHJ_STUBBABLE  \.text\.a6
0+40008 R_MMIX_PUSHJ_STUBBABLE  \.text\.a6\+0x0+4
R.* \[\.text\.b6\]:
O.*
0+40004 R_MMIX_PUSHJ      \.text\.b6
0+40018 R_MMIX_PUSHJ      \.text\.b6\+0x0+4
0+4002c R_MMIX_PUSHJ_STUBBABLE  \.text\.b6\+0x0+8
R.* \[\.text\.c6\]:
O.*
0+ R_MMIX_PUSHJ_STUBBABLE  ca6
0+4 R_MMIX_PUSHJ_STUBBABLE  cb6
R.* \[\.text\.d6\]:
O.*
0+ R_MMIX_PUSHJ      da6
0+14 R_MMIX_PUSHJ      db6
0+28 R_MMIX_PUSHJ_STUBBABLE  dc6
R.* \[\.text\.a7\]:
O.*
0+40004 R_MMIX_PUSHJ_STUBBABLE  \.text\.a7
0+40008 R_MMIX_PUSHJ_STUBBABLE  \.text\.a7\+0x0+4
R.* \[\.text\.b7\]:
O.*
0+40004 R_MMIX_PUSHJ      \.text\.b7
0+40018 R_MMIX_PUSHJ      \.text\.b7\+0x0+4
0+4002c R_MMIX_PUSHJ      \.text\.b7\+0x0+8
R.* \[\.text\.c7\]:
O.*
0+ R_MMIX_PUSHJ_STUBBABLE  ca7
0+4 R_MMIX_PUSHJ_STUBBABLE  cb7
R.* \[\.text\.d7\]:
O.*
0+ R_MMIX_PUSHJ      da7
0+14 R_MMIX_PUSHJ      db7
0+28 R_MMIX_PUSHJ      dc7
R.* \[\.text\.a8\]:
O.*
0+40004 R_MMIX_PUSHJ_STUBBABLE  \.text\.a8
0+40008 R_MMIX_PUSHJ_STUBBABLE  \.text\.a8\+0x0+4
R.* \[\.text\.b8\]:
O.*
0+40004 R_MMIX_PUSHJ      \.text\.b8
0+40018 R_MMIX_PUSHJ      \.text\.b8\+0x0+4
0+4002c R_MMIX_PUSHJ      \.text\.b8\+0x0+8
R.* \[\.text\.c8\]:
O.*
0+ R_MMIX_PUSHJ_STUBBABLE  ca8
0+4 R_MMIX_PUSHJ_STUBBABLE  cb8
R.* \[\.text\.d8\]:
O.*
0+ R_MMIX_PUSHJ      da8
0+14 R_MMIX_PUSHJ      db8
0+28 R_MMIX_PUSHJ      dc8
R.* \[\.text\.a9\]:
O.*
0+40004 R_MMIX_PUSHJ_STUBBABLE  \.text\.a9
0+40008 R_MMIX_PUSHJ_STUBBABLE  \.text\.a9\+0x0+4
R.* \[\.text\.b9\]:
O.*
0+40004 R_MMIX_PUSHJ      \.text\.b9
0+40018 R_MMIX_PUSHJ      \.text\.b9\+0x0+4
0+4002c R_MMIX_PUSHJ      \.text\.b9\+0x0+8
R.* \[\.text\.c9\]:
O.*
0+ R_MMIX_PUSHJ_STUBBABLE  ca9
0+4 R_MMIX_PUSHJ_STUBBABLE  cb9
R.* \[\.text\.d9\]:
O.*
0+ R_MMIX_PUSHJ      da9
0+14 R_MMIX_PUSHJ      db9
0+28 R_MMIX_PUSHJ      dc9
R.* \[\.text\.a10\]:
O.*
0+40004 R_MMIX_PUSHJ_STUBBABLE  \.text\.a10
0+40008 R_MMIX_PUSHJ_STUBBABLE  \.text\.a10\+0x0+4
R.* \[\.text\.b10\]:
O.*
0+40004 R_MMIX_PUSHJ      \.text\.b10
0+40018 R_MMIX_PUSHJ      \.text\.b10\+0x0+4
0+4002c R_MMIX_PUSHJ      \.text\.b10\+0x0+8
R.* \[\.text\.c10\]:
O.*
0+ R_MMIX_PUSHJ_STUBBABLE  ca10
0+4 R_MMIX_PUSHJ_STUBBABLE  cb10
R.* \[\.text\.d10\]:
O.*
0+ R_MMIX_PUSHJ      da10
0+14 R_MMIX_PUSHJ      db10
0+28 R_MMIX_PUSHJ      dc10
R.* \[\.text\.a11\]:
O.*
0+40004 R_MMIX_PUSHJ_STUBBABLE  \.text\.a11
0+40008 R_MMIX_PUSHJ_STUBBABLE  \.text\.a11\+0x0+4
R.* \[\.text\.b11\]:
O.*
0+40004 R_MMIX_PUSHJ_STUBBABLE  \.text\.b11
0+40008 R_MMIX_PUSHJ_STUBBABLE  \.text\.b11\+0x0+4
0+4000c R_MMIX_PUSHJ_STUBBABLE  \.text\.b11\+0x0+8
R.* \[\.text\.c11\]:
O.*
0+ R_MMIX_PUSHJ_STUBBABLE  ca11
0+4 R_MMIX_PUSHJ_STUBBABLE  cb11
R.* \[\.text\.d11\]:
O.*
0+ R_MMIX_PUSHJ_STUBBABLE  da11
0+4 R_MMIX_PUSHJ_STUBBABLE  db11
0+8 R_MMIX_PUSHJ_STUBBABLE  dc11
R.* \[\.text\.a12\]:
O.*
0+40004 R_MMIX_PUSHJ_STUBBABLE  \.text\.a12
0+40008 R_MMIX_PUSHJ_STUBBABLE  \.text\.a12\+0x0+4
R.* \[\.text\.b12\]:
O.*
0+40004 R_MMIX_PUSHJ_STUBBABLE  \.text\.b12
0+40008 R_MMIX_PUSHJ_STUBBABLE  \.text\.b12\+0x0+4
0+4000c R_MMIX_PUSHJ_STUBBABLE  \.text\.b12\+0x0+8
R.* \[\.text\.c12\]:
O.*
0+ R_MMIX_PUSHJ_STUBBABLE  ca12
0+4 R_MMIX_PUSHJ_STUBBABLE  cb12
R.* \[\.text\.d12\]:
O.*
0+ R_MMIX_PUSHJ_STUBBABLE  da12
0+4 R_MMIX_PUSHJ_STUBBABLE  db12
0+8 R_MMIX_PUSHJ_STUBBABLE  dc12
