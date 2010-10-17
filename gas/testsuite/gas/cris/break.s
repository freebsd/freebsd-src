; No-brainer doing an exhaustive test for this one, I guess.
 .text
start:
 break breakpoint
 break 0
 break 1
 break 2
 break 3
 break 4
 break 5
 break 6
 break 7
 break 8
 break 9
 break 10
 break 11
 break 12
 break 13
 break 14
 break 15
end:
 .set breakpoint,2
