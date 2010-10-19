#objdump: -r
#name: ia64 relocations

.*: +file format .*

RELOCATION RECORDS FOR \[\.text\]:
OFFSET[[:space:]]+TYPE[[:space:]]+VALUE[[:space:]]*
[[:xdigit:]]+[012][[:space:]]+IMM14[[:space:]]+esym
[[:xdigit:]]+[012][[:space:]]+IMM22[[:space:]]+esym
[[:xdigit:]]+[012][[:space:]]+IMM64[[:space:]]+esym
[[:xdigit:]]+[012][[:space:]]+GPREL22[[:space:]]+esym
[[:xdigit:]]+[012][[:space:]]+GPREL64I[[:space:]]+esym
[[:xdigit:]]+[012][[:space:]]+LTOFF22[[:space:]]+esym
[[:xdigit:]]+[012][[:space:]]+LTOFF64I[[:space:]]+esym
[[:xdigit:]]+[012][[:space:]]+PLTOFF22[[:space:]]+esym
[[:xdigit:]]+[012][[:space:]]+PLTOFF64I[[:space:]]+esym
[[:xdigit:]]+[012][[:space:]]+FPTR64I[[:space:]]+esym
[[:xdigit:]]+[012][[:space:]]+PCREL60B[[:space:]]+esym
[[:xdigit:]]+[012][[:space:]]+PCREL21B[[:space:]]+esym
[[:xdigit:]]+[012][[:space:]]+PCREL21M[[:space:]]+esym
[[:xdigit:]]+[012][[:space:]]+PCREL21F[[:space:]]+esym
[[:xdigit:]]+[012][[:space:]]+LTOFF_FPTR22[[:space:]]+esym
[[:xdigit:]]+[012][[:space:]]+LTOFF_FPTR64I[[:space:]]+esym
[[:xdigit:]]+[012][[:space:]]+PCREL22[[:space:]]+esym
[[:xdigit:]]+[012][[:space:]]+PCREL64I[[:space:]]+esym
[[:xdigit:]]+[012][[:space:]]+LTOFF22X[[:space:]]+esym
[[:xdigit:]]+[012][[:space:]]+LDXMOV[[:space:]]+esym
[[:xdigit:]]+[012][[:space:]]+TPREL14[[:space:]]+esym
[[:xdigit:]]+[012][[:space:]]+TPREL22[[:space:]]+esym
[[:xdigit:]]+[012][[:space:]]+TPREL64I[[:space:]]+esym
[[:xdigit:]]+[012][[:space:]]+LTOFF_TPREL22[[:space:]]+esym
[[:xdigit:]]+[012][[:space:]]+LTOFF_DTPMOD22[[:space:]]+esym
[[:xdigit:]]+[012][[:space:]]+DTPREL14[[:space:]]+esym
[[:xdigit:]]+[012][[:space:]]+DTPREL22[[:space:]]+esym
[[:xdigit:]]+[012][[:space:]]+DTPREL64I[[:space:]]+esym
[[:xdigit:]]+[012][[:space:]]+LTOFF_DTPREL22[[:space:]]+esym

RELOCATION RECORDS FOR \[\.rodata\.4\]:
OFFSET[[:space:]]+TYPE[[:space:]]+VALUE[[:space:]]*
[[:xdigit:]]+[048cC][[:space:]]+DIR32[LM]SB[[:space:]]+esym
[[:xdigit:]]+[048cC][[:space:]]+GPREL32[LM]SB[[:space:]]+esym
[[:xdigit:]]+[048cC][[:space:]]+FPTR32[LM]SB[[:space:]]+esym
[[:xdigit:]]+[048cC][[:space:]]+PCREL32[LM]SB[[:space:]]+esym
[[:xdigit:]]+[048cC][[:space:]]+LTOFF_FPTR32[LM]SB[[:space:]]+esym
[[:xdigit:]]+[048cC][[:space:]]+SEGREL32[LM]SB[[:space:]]+esym
[[:xdigit:]]+[048cC][[:space:]]+SECREL32[LM]SB[[:space:]]+esym
[[:xdigit:]]+[048cC][[:space:]]+LTV32[LM]SB[[:space:]]+esym
[[:xdigit:]]+[048cC][[:space:]]+DTPREL32[LM]SB[[:space:]]+esym

RELOCATION RECORDS FOR \[\.rodata\.8\]:
OFFSET[[:space:]]+TYPE[[:space:]]+VALUE[[:space:]]*
[[:xdigit:]]+[08][[:space:]]+DIR64[LM]SB[[:space:]]+esym
[[:xdigit:]]+[08][[:space:]]+GPREL64[LM]SB[[:space:]]+esym
[[:xdigit:]]+[08][[:space:]]+PLTOFF64[LM]SB[[:space:]]+esym
[[:xdigit:]]+[08][[:space:]]+FPTR64[LM]SB[[:space:]]+esym
[[:xdigit:]]+[08][[:space:]]+PCREL64[LM]SB[[:space:]]+esym
[[:xdigit:]]+[08][[:space:]]+LTOFF_FPTR64[LM]SB[[:space:]]+esym
[[:xdigit:]]+[08][[:space:]]+SEGREL64[LM]SB[[:space:]]+esym
[[:xdigit:]]+[08][[:space:]]+SECREL64[LM]SB[[:space:]]+esym
[[:xdigit:]]+[08][[:space:]]+LTV64[LM]SB[[:space:]]+esym
[[:xdigit:]]+[08][[:space:]]+IPLT[LM]SB[[:space:]]+esym
[[:xdigit:]]+[08][[:space:]]+TPREL64[LM]SB[[:space:]]+esym
[[:xdigit:]]+[08][[:space:]]+DTPMOD64[LM]SB[[:space:]]+esym
[[:xdigit:]]+[08][[:space:]]+DTPREL64[LM]SB[[:space:]]+esym
