#!/bin/sh
tar zxvf ncftp185.tgz
cd ncftp185
mv Makefile Makefile.ORIG
cvs import src/usr.bin/ncftp mgleason ncftp_1_8_5
