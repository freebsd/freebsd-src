#!/bin/sh
tar zxvf ncftp187.tgz
cd ncftp187
rm Makefile
cvs import src/usr.bin/ncftp mgleason ncftp_1_8_7
