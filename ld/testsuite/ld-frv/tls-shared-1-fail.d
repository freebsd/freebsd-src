#name: FRV TLS relocs, shared linking
#source: tls-1.s
#ld: -shared tmpdir/tls-1-dep.so
#error: different segment
