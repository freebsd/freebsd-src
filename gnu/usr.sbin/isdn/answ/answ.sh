# play.sh
if [ -f /isdn/msg/.num ]
then
	N=`cat /isdn/msg/.num`
else 
	N=0
fi
N=`printf "%.4d" $N`
D=`date +%d%H`
date >> /isdn/msg/I.$N.$D
dd of=/dev/itel00 if=/isdn/msg/msg.answ bs=1k
dd of=/dev/itel00 if=/isdn/msg/beep bs=1k
dd if=/dev/itel00 of=/isdn/msg/R.$N.$D bs=1k
echo `expr $N + 1` >/isdn/msg/.num
