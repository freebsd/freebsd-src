mkdir -p outdir
unifdef -DFOO=1 -DFOOB=42 -UBAR -ooutdir/outfile.c if1.c
e=$?
cat outdir/outfile.c
rm -r outdir
exit $e
