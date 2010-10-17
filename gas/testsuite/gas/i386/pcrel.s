abs = 0x1234
 .extern ext
 .weak weak
 .comm comm,4
 .global glob

 .data
data: .long 0

 .text
loc:
 jmp abs
glob:
 jmp ext
 jmp weak
 jmp comm
 jmp loc
 jmp glob
 jmp abs2
 jmp loc2
 jmp glob2
 jmp data
 jmp data2
 jmp abs   - abs
 jmp ext   - abs
 jmp weak  - abs
 jmp comm  - abs
 jmp loc   - abs
 jmp glob  - abs
 jmp abs2  - abs
 jmp loc2  - abs
 jmp glob2 - abs
 jmp data  - abs
 jmp data2 - abs
 jmp abs   - abs2
 jmp ext   - abs2
 jmp weak  - abs2
 jmp comm  - abs2
 jmp loc   - abs2
 jmp glob  - abs2
 jmp abs2  - abs2
 jmp loc2  - abs2
 jmp glob2 - abs2
 jmp data  - abs2
 jmp data2 - abs2
 jmp loc2  - loc
 jmp glob  - loc
 jmp glob  - loc2
 jmp glob2 - loc
 jmp glob2 - loc2

 .org 0x100
loc2:
 .global glob2
glob2 = loc2 + 5
abs2 = 0x9876

 .data
data2: .long 0
