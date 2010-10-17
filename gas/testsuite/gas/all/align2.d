#objdump: -s -j .text
#name: align2

# Test the section alignment.

.*: .*

Contents of section .text:
 0000 ff[ 	0-9a-f]*[ 	]+.*
