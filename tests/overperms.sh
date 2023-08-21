cp if1.c outfile.c
chmod 640 outfile.c
ls -l outfile.c | cut -d' ' -f1 1>&2
unifdef -DFOO=1 -DFOOB=42 -UBAR -m outfile.c
e=$?
ls -l outfile.c | cut -d' ' -f1 1>&2
cat outfile.c
rm outfile.c
exit $e
