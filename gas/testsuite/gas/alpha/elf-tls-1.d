#objdump: -r
#name: alpha elf-tls-1

.*:     file format elf64-alpha.*

RELOCATION RECORDS FOR \[\.text\]:
OFFSET           TYPE              VALUE 
0*0000004 TLSGD             a
0*0000000 ELF_LITERAL       __tls_get_addr
0*0000008 LITUSE            \.text\+0x0*0000004
0*0000008 HINT              __tls_get_addr
0*000000c HINT              __tls_get_addr
0*0000014 TLSLDM            b
0*0000010 ELF_LITERAL       __tls_get_addr
0*000000c LITUSE            \.text\+0x0*0000005
0*0000018 TLSGD             c
0*000001c TLSLDM            d
0*0000020 TLSGD             e
0*0000024 TLSLDM            f
0*0000028 GOTDTPREL         g
0*000002c DTPRELHI          h
0*0000030 DTPRELLO          i
0*0000034 DTPREL16          j
0*0000038 GOTTPREL          k
0*000003c TPRELHI           l
0*0000040 TPRELLO           m
0*0000044 TPREL16           n


