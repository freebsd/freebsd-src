int
fake_a_gnumalloc_lib()
{
return 1; 
}

void cfree(void *foo)
{
free(foo);
}
