directory /var/named
forwarders 1.2.3.4 1.2.3.5
limit datasize 10000000
limit files 1000
limit transfers-in 100
limit transfers-per-ns 20
; no-round-robin in HP specific
options no-round-robin fake-iquery forward-only no-fetch-glue no-recursion
slave
tcplist 10.0.0.1
xfrnets 10.0.0.2
cache .	rootservers
primary example.net example.net.db
secondary example.com 127.0.0.1 example.com.db
stub example.org 127.0.0.1 example.org.db
primary/IN example.net example.net.db
secondary/IN example.com 127.0.0.1 example.com.db
stub/IN example.org 127.0.0.1 example.org.db
secondary/IN example.com 127.0.0.1
stub/IN example.org 127.0.0.1
primary/CHAOS example.net example.net.db
secondary/CHAOS example.com 127.0.0.1 example.com.db
stub/CHAOS example.org 127.0.0.1 example.org.db
secondary/CHAOS example.com 127.0.0.1
stub/CHAOS example.org 127.0.0.1
primary/HS example.net example.net.db
secondary/HS example.com 127.0.0.1 example.com.db
stub/HS example.org 127.0.0.1 example.org.db
secondary/HS example.com 127.0.0.1
stub/HS example.org 127.0.0.1
