#!/bin/sh

openssl ocsp -index demoCA/index.txt -port 8888 -nmin 5 -rsigner ocsp.pem -rkey ocsp.key -CA demoCA/cacert.pem -text -ignore_err
