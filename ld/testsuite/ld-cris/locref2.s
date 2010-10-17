 .weak expfn
 .weak expobj
y:
 move.d expfn:GOTOFF,$r10
 move.d expobj:GOTOFF,$r11
