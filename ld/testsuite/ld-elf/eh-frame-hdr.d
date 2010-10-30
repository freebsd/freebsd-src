#source: eh-frame-hdr.s
#ld: -e _start --eh-frame-hdr
#objdump: -hw
#target: alpha*-*-*
#target: arm*-*-*
#target: i?86-*-*
#target: m68k-*-*
#target: mips*-*-*
#target: powerpc*-*-*
#target: s390*-*-*
#target: sh*-*-*
#xfail: sh*l*-*-*
#target: sparc*-*-*
#target: x86_64-*-*
#...
  [0-9] .eh_frame_hdr 0*[12][048c] .*
#pass
