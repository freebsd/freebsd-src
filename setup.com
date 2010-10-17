$! setup files for openVMS/Alpha
$!
$ define aout [-.INCLUDE.AOUT]
$ define coff [-.INCLUDE.COFF]
$ define elf [-.INCLUDE.ELF]
$ define mpw [-.INCLUDE.MPW]
$ define nlm [-.INCLUDE.NLM]
$ define opcode [-.INCLUDE.OPCODE]
