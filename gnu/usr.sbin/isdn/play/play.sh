# play.sh
for i in /isdn/msg/R.*
do
	dd of=/dev/itel00 if=$i bs=1k
	dd of=/dev/itel00 if=/isdn/msg/beep bs=1k
done >/dev/null 2>&1
