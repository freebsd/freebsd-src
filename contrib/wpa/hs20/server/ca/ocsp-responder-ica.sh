#!/bin/sh

openssl ocsp -index demoCA/index.txt -port 8888 -nmin 5 -rsigner demoCA/cacert.pem -rkey demoCA/private/cakey-plain.pem -CA demoCA/cacert.pem -resp_no_certs -text
