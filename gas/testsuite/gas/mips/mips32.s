# source file to test assembly of mips32 instructions

        .set noreorder
      .set noat

      .text
text_label:

      # unprivileged CPU instructions

      clo     $1, $2
      clz     $3, $4
      madd    $5, $6
      maddu   $7, $8
      msub    $9, $10
      msubu   $11, $12
      mul     $13, $14, $15
      pref    4, ($16)
      pref    4, 32767($17)
      pref    4, -32768($18)
      ssnop


      # unprivileged coprocessor instructions.
      # these tests use cp2 to avoid other (cp0, fpu, prefetch) opcodes.

      bc2f    text_label
      nop
      bc2fl   text_label
      nop
      bc2t    text_label
      nop
      bc2tl   text_label
      nop
      # XXX other BCzCond encodings not currently expressable
      cfc2    $1, $2
      cop2    0x1234567               # disassembles as c2 ...
      ctc2    $2, $3
      mfc2    $3, $4
      mfc2    $4, $5, 0               # disassembles without sel
      mfc2    $5, $6, 7
      mtc2    $6, $7
      mtc2    $7, $8, 0               # disassembles without sel
      mtc2    $8, $9, 7

      
      # privileged instructions

      cache   5, ($1)
      cache   5, 32767($2)
      cache   5, -32768($3)
      eret
      tlbp
      tlbr
      tlbwi
      tlbwr
      wait
      wait    0                       # disassembles without code
      wait    0x56789

      # For a while break for the mips32 ISA interpreted a single argument
      # as a 20-bit code, placing it in the opcode differently to
      # traditional ISAs.  This turned out to cause problems, so it has
      # been removed.  This test is to assure consistent interpretation.
      break
      break   0                       # disassembles without code
      break   0x345
      break   0x48,0x345              # this still specifies a 20-bit code

      # Instructions in previous ISAs or CPUs which are now slightly
      # different.
      sdbbp
      sdbbp   0                       # disassembles without code
      sdbbp   0x56789

# Force at least 8 (non-delay-slot) zero bytes, to make 'objdump' print ...
      .space  8
