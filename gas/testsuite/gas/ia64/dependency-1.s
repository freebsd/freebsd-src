      .text
      .auto
      .align 32
 
foo:
      ldfs  f8=[r32]
      stfd  [r33]=f8
