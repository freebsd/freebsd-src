#name: Thumb-2-as-Thumb-1 BL failure test
#source: thumb2-bl-as-thumb1-bad.s
#ld: -Ttext 0x1000 --section-start .foo=0x401004
#error: .*\(.text\+0x0\): relocation truncated to fit: R_ARM_THM_CALL against `bar'
