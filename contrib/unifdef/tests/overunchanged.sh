cp if1.c overunchanged.c
ls -i overunchanged.c >overunchanged-before
unifdef -DWEASEL=1 -ooverunchanged.c overunchanged.c
e=$?
ls -i overunchanged.c >overunchanged-after
diff overunchanged-before overunchanged-after
rm -f overunchanged-before overunchanged-after overunchanged.c
exit $e
