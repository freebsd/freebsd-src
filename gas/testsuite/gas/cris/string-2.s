; Test that strings are evaluated as in the manual (slightly modified).
; FIXME:  This should be a generic test.
 .text
start:
 .ascii "This\0x20is a\040string\x20with a \"newline\" at the 'end'"
 .ascii "Megatroid\n", "AX-Foo\r\n"
end:
