d_voidsig='undef'
d_tosignal='int'
gidtype='int'
groupstype='int'
uidtype='int'
# Note that 'Configure' is run from 'UU', hence the strange 'ln'
# command.
for i in .. ../x2p
do
      rm -f $i/3b1cc
      ln ../hints/3b1cc $i
done
echo "\nIf you want to use the 3b1 shared libraries, complete this script then" >&4
echo "read the header in 3b1cc.           [Type carriage return to continue]\c" >&4
read vch
