#objdump: -s -j .data
#name: forward references

.*: .*

Contents of section .data:
 0000 01020304 ff0203fc 01020304 ff0203fc  ................
#pass
