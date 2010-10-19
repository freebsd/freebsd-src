        .section .text
        .global _fun
xc16x_syscontrol:
        srst
        sbrk
        idle
        pwrdn
        srvwdt
        diswdt
        enwdt
        einit

