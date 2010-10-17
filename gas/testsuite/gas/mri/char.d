#objcopy: -O srec
#name: MRI character constants
#as: -M

# Test MRI character constants

S0.*
S113....(61616263616263646500000061276200)|(61616263646362610000006500622761).*
#pass
