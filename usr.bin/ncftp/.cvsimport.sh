#!/bin/sh
tar zxvf ncftp-1.9.2.tgz
cd ncftp-1.9.2
rm Makefile
cvs import src/usr.bin/ncftp mgleason ncftp_1_9_2
