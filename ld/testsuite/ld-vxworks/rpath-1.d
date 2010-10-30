# source: rpath-1.s
# ld: --entry foo --rpath /dir1 --rpath /dir2 --rpath net:/dir4 --rpath /dir2 --rpath /dir1 --rpath /dir3 --force-dynamic -q
# readelf: -d
#...
 0x0+f \(RPATH\).*Library rpath: \[/dir1;/dir2;net:/dir4;/dir3\]
#pass
