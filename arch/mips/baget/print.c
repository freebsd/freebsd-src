/*
 * print.c: Simple print fascility
 *
 * Copyright (C) 1998 Gleb Raiko & Vladimir Roganov
 */
#include <stdarg.h>
#include <linux/kernel.h>
#include <linux/init.h>

#include <asm/baget/baget.h>

/*
 *  Define this to see 'baget_printk' (debug) messages
 */
// #define BAGET_PRINTK

/*
 *  This function is same for BALO and Linux baget_printk,
 *  and normally prints characted to second (UART A) console.
 */

static void delay(void) {}

static void outc_low(char c)
{
        int i;
        vac_outb(c, VAC_UART_B_TX);
        for (i=0; i<10000; i++)
                delay();
}

void outc(char c)
{
        if (c == '\n')
                outc_low('\r');
        outc_low(c);
}

void outs(char *s)
{
        while(*s) outc(*s++);
}

void baget_write(char *s, int l)
{
        while(l--)
                outc(*s++);
}

int baget_printk(const char *fmt, ...)
{
#ifdef BAGET_PRINTK
        va_list args;
        int i;
        static char buf[1024];

        va_start(args, fmt);
        i = vsprintf(buf, fmt, args); /* hopefully i < sizeof(buf)-4 */
        va_end(args);
        baget_write(buf, i);
        return i;
#else
	return 0;
#endif
}

static __inline__ void puthex( int a )
{
        static char s[9];
        static char e[] = "0123456789ABCDEF";
        int i;
        for( i = 7; i >= 0; i--, a >>= 4 ) s[i] = e[a & 0x0F];
        s[8] = '\0';
        outs( s );
}

void __init balo_printf( char *f, ... )
{
        int *arg = (int*)&f + 1;
        char c;
        int format = 0;

        while((c = *f++) != 0) {
                switch(c) {
                default:
                        if(format) {
                                outc('%');
                                format = 0;
                        }
                        outc( c );
                        break;
                case '%':
                        if( format ){
                                format = 0;
                                outc(c);
                        } else format = 1;
                        break;
                case 'x':
                        if(format) puthex( *arg++ );
                        else outc(c);
                        format = 0;
                        break;
                case 's':
                        if( format ) outs((char *)*arg++);
                        else outc(c);
                        format = 0;
                        break;
                }
        }
}

void __init balo_hungup(void)
{
        outs("Hunging up.\n");
        while(1);
}
