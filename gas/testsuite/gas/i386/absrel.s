abs = 0x1234
 .extern ext
 .weak weak
 .comm comm,4
 .global glob

 .data
data: .long 0

 .text
loc:
 mov abs,  %eax
glob:
 mov ext,  %eax
 mov weak, %eax
 mov comm, %eax
 mov loc,  %eax
 mov glob, %eax
 mov abs2, %eax
 mov loc2, %eax
 mov glob2,%eax
 mov data, %eax
 mov data2,%eax
 mov abs   - abs, %eax
 mov ext   - abs, %eax
 mov weak  - abs, %eax
 mov comm  - abs, %eax
 mov loc   - abs, %eax
 mov glob  - abs, %eax
 mov abs2  - abs, %eax
 mov loc2  - abs, %eax
 mov glob2 - abs, %eax
 mov data  - abs, %eax
 mov data2 - abs, %eax
 mov abs   - abs2,%eax
 mov ext   - abs2,%eax
 mov weak  - abs2,%eax
 mov comm  - abs2,%eax
 mov loc   - abs2,%eax
 mov glob  - abs2,%eax
 mov abs2  - abs2,%eax
 mov loc2  - abs2,%eax
 mov glob2 - abs2,%eax
 mov data  - abs2,%eax
 mov data2 - abs2,%eax
 mov loc2  - loc, %eax
 mov glob  - loc, %eax
 mov glob  - loc2,%eax
 mov glob2 - loc, %eax
 mov glob2 - loc2,%eax

 .org 0x100
loc2:
 .global glob2
glob2 = loc2 + 5
abs2 = 0x9876

 .data
data2: .long 0
