        .text
        .global iwmmxt
iwmmxt:

       wldrb   wr1, [r1], #0
       wldrh   wr1, [r1], #0
       wstrb   wr1, [r1], #0
       wstrh   wr1, [r1], #0
