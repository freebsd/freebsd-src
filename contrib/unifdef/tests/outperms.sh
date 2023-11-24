umask 027
unifdef -DFOO=1 -DFOOB=42 -UBAR -ooutfile.c if1.c
e=$?
case ${BUILD_MINGW} in
(yes)	printf '%s\n' '-rw-r-----' 1>&2 ;;
(*)	ls -l outfile.c | cut -d' ' -f1 1>&2 ;;
esac
cat outfile.c
rm outfile.c
exit $e
