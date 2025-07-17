#service ip6addrctl prefer_ipv4
TEST=./h_gai
family=v4_only

( $TEST ::1 http
  $TEST 127.0.0.1 http
  $TEST localhost http
  $TEST ::1 tftp
  $TEST 127.0.0.1 tftp
  $TEST localhost tftp
  $TEST ::1 echo
  $TEST 127.0.0.1 echo
  $TEST localhost echo ) > basics_${family}.exp

( $TEST -4 localhost http
  $TEST -6 localhost http ) > spec_fam_${family}.exp

( $TEST '' http
  $TEST '' echo
  $TEST '' tftp
  $TEST '' 80
  $TEST -P '' http
  $TEST -P '' echo
  $TEST -P '' tftp
  $TEST -P '' 80
  $TEST -S '' 80
  $TEST -D '' 80 ) > no_host_${family}.exp

( $TEST ::1 ''
  $TEST 127.0.0.1 ''
  $TEST localhost ''
  $TEST '' '' ) > no_serv_${family}.exp

( $TEST -R -p 0 localhost ''
  $TEST -R -p 59 localhost ''
  $TEST -R -p 59 localhost 80
  $TEST -R -p 59 localhost www
  $TEST -R -p 59 ::1 '' ) > sock_raw_${family}.exp

( $TEST -f 99 localhost '' ) > unsup_fam_${family}.exp

( $TEST fe80::1%lo0 http
#	  IF=`ifconfig -a | grep -v '^	' | sed -e 's/:.*//' | head -1 | awk '{print $1}'`
#	  $TEST fe80::1%$IF http
) > scoped_${family}.exp
