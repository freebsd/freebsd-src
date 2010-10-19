#objdump: -r
#name: weakref tests, relocations
# ecoff (OSF/alpha) lacks .weak support
# pdp11 lacks .long
# the following must be present in all weakref1*.d
#not-target: alpha*-*-osf* *-*-ecoff pdp11-*-aout

#...
RELOCATION RECORDS FOR \[(\.text|\$CODE\$)\]:
OFFSET +TYPE +VALUE *
# the rest of this file is generated with the following script:
# # script begin
# echo \#...
# sed -n 's:^[ 	]*\.long \(W\|\)\(.*[^a-z]\)[a-z]*\(\| - .*\)$:\2:p' weakref1.s | sed -e 's,^[lg].*,(&|\\.text)(\\+0x[0-9a-f]+)?,' | sed 's,^,[0-9a-f]+ [^ ]*  +,'
# # script output:
#...
[0-9a-f]+ [^ ]*  +wa1
[0-9a-f]+ [^ ]*  +ua2
[0-9a-f]+ [^ ]*  +ua3
[0-9a-f]+ [^ ]*  +ua3
[0-9a-f]+ [^ ]*  +ua4
[0-9a-f]+ [^ ]*  +ua4
[0-9a-f]+ [^ ]*  +wb1
[0-9a-f]+ [^ ]*  +ub2
[0-9a-f]+ [^ ]*  +ub3
[0-9a-f]+ [^ ]*  +ub3
[0-9a-f]+ [^ ]*  +ub4
[0-9a-f]+ [^ ]*  +ub4
[0-9a-f]+ [^ ]*  +wc1
[0-9a-f]+ [^ ]*  +wc1
[0-9a-f]+ [^ ]*  +uc2
[0-9a-f]+ [^ ]*  +uc2
[0-9a-f]+ [^ ]*  +uc3
[0-9a-f]+ [^ ]*  +uc3
[0-9a-f]+ [^ ]*  +uc3
[0-9a-f]+ [^ ]*  +uc3
[0-9a-f]+ [^ ]*  +uc4
[0-9a-f]+ [^ ]*  +uc4
[0-9a-f]+ [^ ]*  +uc4
[0-9a-f]+ [^ ]*  +uc4
[0-9a-f]+ [^ ]*  +uc5
[0-9a-f]+ [^ ]*  +uc5
[0-9a-f]+ [^ ]*  +uc5
[0-9a-f]+ [^ ]*  +uc5
[0-9a-f]+ [^ ]*  +uc6
[0-9a-f]+ [^ ]*  +uc6
[0-9a-f]+ [^ ]*  +uc6
[0-9a-f]+ [^ ]*  +uc6
[0-9a-f]+ [^ ]*  +uc7
[0-9a-f]+ [^ ]*  +uc7
[0-9a-f]+ [^ ]*  +uc8
[0-9a-f]+ [^ ]*  +uc8
[0-9a-f]+ [^ ]*  +uc9
[0-9a-f]+ [^ ]*  +uc9
[0-9a-f]+ [^ ]*  +uc9
[0-9a-f]+ [^ ]*  +ww1
[0-9a-f]+ [^ ]*  +ww2
[0-9a-f]+ [^ ]*  +ww3
[0-9a-f]+ [^ ]*  +ww3
[0-9a-f]+ [^ ]*  +ww4
[0-9a-f]+ [^ ]*  +ww4
[0-9a-f]+ [^ ]*  +ww5
[0-9a-f]+ [^ ]*  +ww5
[0-9a-f]+ [^ ]*  +ww6
[0-9a-f]+ [^ ]*  +ww7
[0-9a-f]+ [^ ]*  +ww8
[0-9a-f]+ [^ ]*  +ww8
[0-9a-f]+ [^ ]*  +ww9
[0-9a-f]+ [^ ]*  +ww9
[0-9a-f]+ [^ ]*  +ww10
[0-9a-f]+ [^ ]*  +ww10
[0-9a-f]+ [^ ]*  +um5
[0-9a-f]+ [^ ]*  +wm6
[0-9a-f]+ [^ ]*  +wm7
[0-9a-f]+ [^ ]*  +wm8
[0-9a-f]+ [^ ]*  +wh2
[0-9a-f]+ [^ ]*  +wh3
[0-9a-f]+ [^ ]*  +wh4
[0-9a-f]+ [^ ]*  +wh5
[0-9a-f]+ [^ ]*  +wh6
[0-9a-f]+ [^ ]*  +wh7
[0-9a-f]+ [^ ]*  +uh8
[0-9a-f]+ [^ ]*  +uh8
[0-9a-f]+ [^ ]*  +uh9
[0-9a-f]+ [^ ]*  +uh9
[0-9a-f]+ [^ ]*  +(ld1|\.text|\$CODE\$)(\+0x[0-9a-f]+)?
[0-9a-f]+ [^ ]*  +(ld2|\.text|\$CODE\$)(\+0x[0-9a-f]+)?
[0-9a-f]+ [^ ]*  +(ld3|\.text|\$CODE\$)(\+0x[0-9a-f]+)?
[0-9a-f]+ [^ ]*  +(ld4|\.text|\$CODE\$)(\+0x[0-9a-f]+)?
[0-9a-f]+ [^ ]*  +ud5
[0-9a-f]+ [^ ]*  +(gd6|\.text|\$CODE\$)(\+0x[0-9a-f]+)?
[0-9a-f]+ [^ ]*  +(gd7|\.text|\$CODE\$)(\+0x[0-9a-f]+)?
[0-9a-f]+ [^ ]*  +(ld8|\.text|\$CODE\$)(\+0x[0-9a-f]+)?
[0-9a-f]+ [^ ]*  +(ld8|\.text|\$CODE\$)(\+0x[0-9a-f]+)?
[0-9a-f]+ [^ ]*  +(ld9|\.text|\$CODE\$)(\+0x[0-9a-f]+)?
[0-9a-f]+ [^ ]*  +(ld9|\.text|\$CODE\$)(\+0x[0-9a-f]+)?
