#	@(#)TEST.csh	5.2 (Berkeley) 4/30/93
#	$FreeBSD$

#alias t '/usr/src/bin/test/obj/test \!*; echo $status'
alias t '/bin/test \!*; echo $status'

echo 't -b /dev/ttyp2'
t -b /dev/ttyp2
echo 't -b /dev/jb1a'
t -b /dev/jb1a

echo 't -c test.c'
t -c test.c
echo 't -c /dev/tty'
t -c /dev/tty

echo 't -d test.c'
t -d test.c
echo 't -d /etc'
t -d /etc

echo 't -e noexist'
t -e noexist
echo 't -e test.c'
t -e test.c

echo 't -f noexist'
t -f noexist
echo 't -f /dev/tty'
t -f /dev/tty
echo 't -f test.c'
t -f test.c

echo 't -g test.c'
t -g test.c
echo 't -g /bin/ps'
t -g /bin/ps

echo 't -n ""'
t -n ""
echo 't -n "hello"'
t -n "hello"

echo 't -p test.c'
t -p test.c

echo 't -r noexist'
t -r noexist
echo 't -r /etc/master.passwd'
t -r /etc/master.passwd
echo 't -r test.c'
t -r test.c

echo 't -s noexist'
t -s noexist
echo 't -s /dev/null'
t -s /dev/null
echo 't -s test.c'
t -s test.c

echo 't -t 20'
t -t 20
echo 't -t 0'
t -t 0

echo 't -u test.c'
t -u test.c
echo 't -u /bin/rcp'
t -u /bin/rcp

echo 't -w noexist'
t -w noexist
echo 't -w /etc/master.passwd'
t -w /etc/master.passwd
echo 't -w /dev/null'
t -w /dev/null

echo 't -x noexist'
t -x noexist
echo 't -x /bin/ps'
t -x /bin/ps
echo 't -x /etc/motd'
t -x /etc/motd

echo 't -z ""'
t -z ""
echo 't -z "foo"'
t -z "foo"

echo 't "foo"'
t "foo"
echo 't ""'
t ""

echo 't "hello" = "hello"'
t "hello" = "hello"
echo 't "hello" = "goodbye"'
t "hello" = "goodbye"

echo 't "hello" != "hello"'
t "hello" != "hello"
echo 't "hello" != "goodbye"'
t "hello" != "goodbye"

echo 't 200 -eq 200'
t 200 -eq 200
echo 't 34 -eq 222'
t 34 -eq 222

echo 't 200 -ne 200'
t 200 -ne 200
echo 't 34 -ne 222'
t 34 -ne 222

echo 't 200 -gt 200'
t 200 -gt 200
echo 't 340 -gt 222'
t 340 -gt 222

echo 't 200 -ge 200'
t 200 -ge 200
echo 't 34 -ge 222'
t 34 -ge 222

echo 't 200 -lt 200'
t 200 -lt 200
echo 't 34 -lt 222'
t 34 -lt 222

echo 't 200 -le 200'
t 200 -le 200
echo 't 340 -le 222'
t 340 -le 222

echo 't 700 -le 1000 -a -n "1" -a "20" = "20"'
t 700 -le 1000 -a -n "1" -a "20" = "20"
echo 't ! \( 700 -le 1000 -a -n "1" -a "20" = "20" \)'
t ! \( 700 -le 1000 -a -n "1" -a "20" = "20" \)

echo 't -5 -eq 5'
t -5 -eq 5


echo 't foo -a ""'
t foo -a ""
echo 't "" -a foo'
t "" -a foo
echo 't "" -a ""'
t "" -a ""
echo 't "" -o ""'
t "" -o ""

