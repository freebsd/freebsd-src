\ Example of the file which is automatically loaded by /boot/loader
\ on startup.
\ $Id: boot.4th,v 1.1 1998/12/22 12:15:45 abial Exp $

\ Load the screen manipulation words

cr .( Loading Forth extensions:)

cr .( - screen.4th...)
s" /boot/screen.4th" fopen dup fload fclose

\ Load frame support
cr .( - frames.4th...)
s" /boot/frames.4th" fopen dup fload fclose

\ Load our little menu
cr .( - menu.4th...)
s" /boot/menu.4th" fopen dup fload fclose

\ Show it
cr
main_menu
