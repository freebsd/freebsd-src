#source: merge.s
#ld: -T merge.ld
#objdump: -s
#xfail: "arc-*-*" "avr-*-*" "cris-*-*" "dlx-*-*" "fr30-*-*" "frv-*-*"
#xfail: "hppa*-*-*" "h8300-*-*" "i960-*-*" "ip2k-*-*" "m32r-*-*" "mcore-*-*"
#xfail: "mn10*-*-*" "mips64*-*-linux*" "openrisc-*-*" "pj-*-*" "sparc*-*-*"
#xfail: "xtensa-*-*"

.*:     file format .*elf.*

Contents of section .text:
 1000 (1010)?0000(1010)? (1210)?0000(1012)? (0c)?000000(0c)? (0e)?000000(0e)? .*
Contents of section .rodata:
 1010 61626300 .*
#pass
