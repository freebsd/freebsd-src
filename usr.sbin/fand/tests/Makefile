PACKAGE=	tests

ATF_TESTS_SH+=	fand_test

${PACKAGE}FILES+=	celsius.cprofile celsius.expout
${PACKAGE}FILES+=	millikelvins.cprofile millikelvins.expout

.for t in badprop misordered-steps missing-duty missing-loduty missing-steps missing-temp too-many-steps
${PACKAGE}FILES+=	${t}.cprofile ${t}.experr
.endfor

.include <bsd.test.mk>
