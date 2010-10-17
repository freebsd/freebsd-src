
    ; Test case for assembler option "itbl".
    ; Run as "as --itbl itbl itbl.s"
    ; or with stand-alone test case "itbl-test itbl itbl.s".

    ; Assemble processor instructions as defined in "itbl".

    fee $d3,$c2,0x1 	;  0x4ff07601
    fie 	  	;  0x4ff00000
    foh $2,0x100
    fum $d3,$c2     	;  0x4ff07601
    pig $2,0x100

