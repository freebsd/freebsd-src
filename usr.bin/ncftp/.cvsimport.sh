#!/bin/sh
tar zxvf ncftp-1.9.0.tgz
cd ncftp-1.9.0
rm Makefile
cvs import src/usr.bin/ncftp mgleason ncftp_1_9_0
