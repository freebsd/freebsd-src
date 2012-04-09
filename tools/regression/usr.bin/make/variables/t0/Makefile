# $FreeBSD: src/tools/regression/usr.bin/make/variables/t0/Makefile,v 1.1.22.1.8.1 2012/03/03 06:15:13 kensmith Exp $
#
# A unsorted set of macro expansions. This should be sorted out.
#
A	= 0
AV	= 1
test1:
	@echo $A
	@echo ${AV}
	@echo ${A}
	@# The following are soo broken why no syntax error?
	@echo $(
	@echo $)
	@echo ${
	@echo ${A
	@echo ${A)
	@echo ${A){
	@echo ${AV
	@echo ${AV)
	@echo ${AV){
	@echo ${AV{
	@echo ${A{
	@echo $}
