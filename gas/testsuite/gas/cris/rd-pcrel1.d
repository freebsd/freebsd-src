#objdump: -dr

.*:     file format .*-cris

Disassembly of section \.text:
#...
[ 	]+28:[ 	]+6fae 6e00 0000[ 	]+move.d 6e <y03\+0xa>,\$?r10
[ 	]+2e:[ 	]+6fae dcff ffff[ 	]+move.d 0xffffffdc,\$?r10
[ 	]+34:[ 	]+6fae 5c00 0000[ 	]+move.d 5c <y01>,\$?r10
[ 	]+3a:[ 	]+6fae caff ffff[ 	]+move.d 0xffffffca,\$?r10
[ 	]+40:[ 	]+6fae 5000 0000[ 	]+move.d 50 <xx\+0x28>,\$?r10
[ 	]+46:[ 	]+6fae beff ffff[ 	]+move.d 0xffffffbe,\$?r10
[ 	]+4c:[ 	]+3ef1 633a[ 	]+move.d \[\$?pc\+62\],\$?r3
[ 	]+50:[ 	]+b8f1 633a[ 	]+move.d \[\$?pc-72\],\$?r3
[ 	]+54:[ 	]+40f1 633a[ 	]+move.d \[\$?pc\+64\],\$?r3
0+58 <y00>:
[ 	]+58:[ 	]+b0f1 633a[ 	]+move.d \[\$?pc-80\],\$?r3
0+5c <y01>:
[ 	]+5c:[ 	]+38f1 633a[ 	]+move.d \[\$?pc\+56\],\$?r3
0+60 <y02>:
[ 	]+60:[ 	]+a8f1 633a[ 	]+move.d \[\$?pc-88\],\$?r3
0+64 <y03>:
#...
[ 	]+480:[ 	]+5ffd 0a04 633a[ 	]+move.d \[\$?pc\+1034\],\$?r3
[ 	]+486:[ 	]+5ffd 82fb 633a[ 	]+move.d \[\$?pc-1150\],\$?r3
[ 	]+48c:[ 	]+5ffd fc03 633a[ 	]+move.d \[\$?pc\+1020\],\$?r3
0+492 <yy00>:
[ 	]+492:[ 	]+5ffd 74fb 633a[ 	]+move.d \[\$?pc-1164\],\$?r3
0+498 <yy01>:
[ 	]+498:[ 	]+5ffd f003 633a[ 	]+move.d \[\$?pc\+1008\],\$?r3
0+49e <yy02>:
[ 	]+49e:[ 	]+5ffd 68fb 633a[ 	]+move.d \[\$?pc-1176\],\$?r3
#...
[ 	]+18f2e:[ 	]+6ffd ce86 0100 633a[ 	]+move.d \[\$?pc\+186ce <yy\+0x17e42>\],\$?r3
[ 	]+18f36:[ 	]+6ffd d270 feff 633a[ 	]+move.d \[\$?pc\+fffe70d2 <z\+0xfffb5ad4>\],\$?r3
[ 	]+18f3e:[ 	]+6ffd ba86 0100 633a[ 	]+move.d \[\$?pc\+186ba <yy\+0x17e2e>\],\$?r3
0+18f46 <z00>:
[ 	]+18f46:[ 	]+6ffd be70 feff 633a[ 	]+move.d \[\$?pc\+fffe70be <z\+0xfffb5ac0>\],\$?r3
0+18f4e <z01>:
[ 	]+18f4e:[ 	]+6ffd aa86 0100 633a[ 	]+move.d \[\$?pc\+186aa <yy\+0x17e1e>\],\$?r3
0+18f56 <z02>:
[ 	]+18f56:[ 	]+6ffd ae70 feff 633a[ 	]+move.d \[\$?pc\+fffe70ae <z\+0xfffb5ab0>\],\$?r3
#...
0+315fe <z>:
[ 	]+315fe:[ 	]+0f05[ 	]+nop 
