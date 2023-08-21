umask 027
unifdef -DFOO=1 -DFOOB=42 -UBAR -ooutfile.c if1.c
e=$?
ls -l outfile.c | cut -d' ' -f1 1>&2
cat outfile.c
rm outfile.c
exit $e
