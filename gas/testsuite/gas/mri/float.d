#objcopy: -O srec
#name: MRI floating point constants
#as: -M

# Test MRI floating point constants

S0.*
S113....(123456789ABCDEF03F80000041200000)|(F0DEBC9A785634120000803F00002041).*
S10.....(4120000042C80000)|(000020410000C842).*
#pass
