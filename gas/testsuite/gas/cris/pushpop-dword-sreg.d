#objdump: -dr
#name: pushpop-dword-sreg-@OR@

.*:     file format .*-cris

Disassembly of section \.text:
0+ <start>:
^[ 	]+0:[ 	]+fce1 @IM+7e0e@[ 	]+push[ ]+[\$a-z].*
^[ 	]+4:[ 	]+fce1 @IM+7e0e@[ 	]+push[ ]+[\$a-z].*
^[ 	]+8:[ 	]+fce1 @IM+7e0e@[ 	]+push[ ]+[\$a-z].*
^[ 	]+c:[ 	]+f8e1 @IM+7e0e@[ 	]+(clear.d |move[ ]+[^,]+,)\[\$?sp=\$?sp-8\]
^[ 	]+10:[ 	]+fbe1 @IM+7e0e@[ 	]+(clear.d |move[ ]+[^,]+,)\[\$?sp=\$?sp-5\]
^[ 	]+14:[ 	]+fde1 @IM+7e0e@[ 	]+(clear.d |move[ ]+[^,]+,)\[\$?sp=\$?sp-3\]
^[ 	]+18:[ 	]+fee1 @IM+7e0e@[ 	]+(clear.d |move[ ]+[^,]+,)\[\$?sp=\$?sp-2\]
^[ 	]+1c:[ 	]+ffe1 @IM+7e0e@[ 	]+(clear.d |move[ ]+[^,]+,)\[\$?sp=\$?sp-1\]
^[ 	]+20:[ 	]+00e1 @IM+7e0e@[ 	]+(clear.d |move[ ]+[^,]+,)\[\$?sp=\$?sp\+0\]
^[ 	]+24:[ 	]+01e1 @IM+7e0e@[ 	]+(clear.d |move[ ]+[^,]+,)\[\$?sp=\$?sp\+1\]
^[ 	]+28:[ 	]+02e1 @IM+7e0e@[ 	]+(clear.d |move[ ]+[^,]+,)\[\$?sp=\$?sp\+2\]
^[ 	]+2c:[ 	]+03e1 @IM+7e0e@[ 	]+(clear.d |move[ ]+[^,]+,)\[\$?sp=\$?sp\+3\]
^[ 	]+30:[ 	]+04e1 @IM+7e0e@[ 	]+(clear.d |move[ ]+[^,]+,)\[\$?sp=\$?sp\+4\]
^[ 	]+34:[ 	]+05e1 @IM+7e0e@[ 	]+(clear.d |move[ ]+[^,]+,)\[\$?sp=\$?sp\+5\]
^[ 	]+38:[ 	]+08e1 @IM+7e0e@[ 	]+(clear.d |move[ ]+[^,]+,)\[\$?sp=\$?sp\+8\]
^[ 	]+3c:[ 	]+08e1 @IM+3e0e@[ 	]+move[ ]+\[\$?sp=\$?sp\+8\],.*
^[ 	]+40:[ 	]+05e1 @IM+3e0e@[ 	]+move[ ]+\[\$?sp=\$?sp\+5\],.*
^[ 	]+44:[ 	]+04e1 @IM+3e0e@[ 	]+move[ ]+\[\$?sp=\$?sp\+4\],.*
^[ 	]+48:[ 	]+03e1 @IM+3e0e@[ 	]+move[ ]+\[\$?sp=\$?sp\+3\],.*
^[ 	]+4c:[ 	]+02e1 @IM+3e0e@[ 	]+move[ ]+\[\$?sp=\$?sp\+2\],.*
^[ 	]+50:[ 	]+01e1 @IM+3e0e@[ 	]+move[ ]+\[\$?sp=\$?sp\+1\],.*
^[ 	]+54:[ 	]+00e1 @IM+3e0e@[ 	]+move[ ]+\[\$?sp=\$?sp\+0\],.*
^[ 	]+58:[ 	]+ffe1 @IM+3e0e@[ 	]+move[ ]+\[\$?sp=\$?sp-1\],.*
^[ 	]+5c:[ 	]+fee1 @IM+3e0e@[ 	]+move[ ]+\[\$?sp=\$?sp-2\],.*
^[ 	]+60:[ 	]+fde1 @IM+3e0e@[ 	]+move[ ]+\[\$?sp=\$?sp-3\],.*
^[ 	]+64:[ 	]+fce1 @IM+3e0e@[ 	]+move[ ]+\[\$?sp=\$?sp-4\],.*
^[ 	]+68:[ 	]+fbe1 @IM+3e0e@[ 	]+move[ ]+\[\$?sp=\$?sp-5\],.*
^[ 	]+6c:[ 	]+f8e1 @IM+3e0e@[ 	]+move[ ]+\[\$?sp=\$?sp-8\],.*
^[ 	]+70:[ 	]+fce1 @IM+7e0e@[ 	]+push[ ]+[\$a-z].*
^[ 	]+74:[ 	]+@IM+3e0e@[ 	]+pop[ ]+[\$a-z].*
^[ 	]+76:[ 	]+@IM+3e0e@[ 	]+pop[ ]+[\$a-z].*
^[ 	]+78:[ 	]+fce1 @IM+7e0e@[ 	]+push[ ]+[\$a-z].*
