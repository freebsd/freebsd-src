 .altmacro

 .macro x.y.z val
  .align 4
  .byte val, val
 .endm

 .macro .xyz val
  .align 8
  .byte val, val
 .endm

 .macro .macro
 .endm

label1:label2 : label3 :label4: m: .macro arg.1, arg.2
 .data
labelA:labelB : labelC :labelD: x.y.z arg.1+arg.2
 .skip arg.2
labelZ:labelY : labelX :labelW: .xyz arg.1-arg.2
 .skip arg.1*arg.2
label9:label8 : label7 :label6: .endm

m 4, 2

 .purgem .xyz, x.y.z
 .xyz 0
x.y.z 0
