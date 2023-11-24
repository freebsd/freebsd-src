echo undef
if ! unifdef div.c
then unifdef -d div.c
fi
echo one
unifdef -DDENOM div.c
echo zero
if ! unifdef -UDENOM div.c
then unifdef -d -UDENOM div.c
fi
