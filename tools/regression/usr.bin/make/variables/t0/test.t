#!/bin/sh

# $FreeBSD$

cd `dirname $0`
. ../../common.sh

setup_test()
{
	cat > ${WORK_DIR}/Makefile << "_EOF_"
A	= 0
AV	= 1
all:
	echo $A
	echo ${AV}
	echo ${A}
	# The following are soo broken why no syntax error?
	echo $(
	echo $)
	echo ${
	echo ${A
	echo ${A)
	echo ${A){
	echo ${AV
	echo ${AV)
	echo ${AV){
	echo ${AV{
	echo ${A{
	echo $}
_EOF_
}

desc_test()
{
	echo "Variable expansion."
}

eval_cmd $1
