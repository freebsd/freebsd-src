#!/bin/sh
tar zxvf ncftp186.tgz
cd ncftp186
rm Makefile
cvs import src/usr.bin/ncftp mgleason ncftp_1_8_6
