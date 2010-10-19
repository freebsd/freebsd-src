#objdump: -r
#name: ia64 unwind relocations (ilp32)
#as: -milp32
#source: reloc-uw.s

.*: +file format .*

RELOCATION RECORDS FOR \[\.IA_64\.unwind\]:
OFFSET[[:space:]]+TYPE[[:space:]]+VALUE[[:space:]]*
0*00 SEGREL32[ML]SB[[:space:]]+\.text(\+0x[[:xdigit:]]*0)?
0*04 SEGREL32[ML]SB[[:space:]]+\.text(\+0x[[:xdigit:]]*0)?
0*08 SEGREL32[ML]SB[[:space:]]+\.IA_64\.unwind_info(\+0x[[:xdigit:]]*[048c])?
0*0c SEGREL32[ML]SB[[:space:]]+\.text(\+0x[[:xdigit:]]*0)?
0*10 SEGREL32[ML]SB[[:space:]]+\.text(\+0x[[:xdigit:]]*0)?
0*14 SEGREL32[ML]SB[[:space:]]+\.IA_64\.unwind_info(\+0x[[:xdigit:]]*[048c])?
