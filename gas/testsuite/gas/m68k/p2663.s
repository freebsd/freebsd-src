|
| This code generates an incorrect pc relative offset 
|
bug:       movel  #4,%d7
           jsr    table(%pc,%d7.w)            | wrong
           jsr    %pc@(table-.-2:b,%d7:w)     | correct but cryptic
           nop
           nop
table:
           bra    junk
           bra    junk
           bra    junk

junk:
    nop
    rts
