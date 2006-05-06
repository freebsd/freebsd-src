\ Example of the file which is automatically loaded by /boot/loader
\ on startup.
\ $FreeBSD: src/share/examples/bootforth/boot.4th,v 1.3 2001/12/11 00:49:33 jhb Exp $

\ Load the screen manipulation words

cr .( Loading Forth extensions:)

cr .( - screen.4th...)
s" /boot/screen.4th" O_RDONLY fopen dup fload fclose

\ Load frame support
cr .( - frames.4th...)
s" /boot/frames.4th" O_RDONLY fopen dup fload fclose

\ Load our little menu
cr .( - menu.4th...)
s" /boot/menu.4th" O_RDONLY fopen dup fload fclose

\ Show it
cr
main_menu
