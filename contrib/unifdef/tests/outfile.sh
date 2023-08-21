unifdef -DFOO=1 -DFOOB=42 -UBAR -ooutfile.c if1.c
e=$?
cat outfile.c
rm outfile.c
exit $e
