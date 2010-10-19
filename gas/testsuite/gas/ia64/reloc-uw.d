# objdump: -r
# name: ia64 unwind relocations

.*: +file format .*

RELOCATION RECORDS FOR \[\.IA_64\.unwind\]:
OFFSET[[:space:]]+TYPE[[:space:]]+VALUE[[:space:]]*
0*00 SEGREL64[ML]SB[[:space:]]+\.text(\+0x[[:xdigit:]]*0)?
0*08 SEGREL64[ML]SB[[:space:]]+\.text(\+0x[[:xdigit:]]*0)?
0*10 SEGREL64[ML]SB[[:space:]]+\.IA_64\.unwind_info(\+0x[[:xdigit:]]*[08])?
0*18 SEGREL64[ML]SB[[:space:]]+\.text(\+0x[[:xdigit:]]*0)?
0*20 SEGREL64[ML]SB[[:space:]]+\.text(\+0x[[:xdigit:]]*0)?
0*28 SEGREL64[ML]SB[[:space:]]+\.IA_64\.unwind_info(\+0x[[:xdigit:]]*[08])?
