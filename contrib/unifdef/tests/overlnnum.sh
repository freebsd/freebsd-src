cp if1.c overwrite.c
unifdef -DFOO=1 -DFOOB=42 -UBAR -n -ooverwrite.c overwrite.c
e=$?
cat overwrite.c
rm overwrite.c
exit $e
