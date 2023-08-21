mkdir -p outdir
chmod 555 outdir
unifdef -DFOO=1 -DFOOB=42 -UBAR -ooutdir/outfile.c if1.c
e=$?
rmdir outdir
exit $e
