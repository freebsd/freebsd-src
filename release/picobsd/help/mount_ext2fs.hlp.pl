[1mmount_ext2fs[m	zamontuj system plikow EXT2FS (Linux)

	Sposob uzycia:

	mount_ext2fs [-o opcje] urzadzenie punkt

	N.p. zeby zamontowac pierwsza partycje na pierwszym dysku IDE:

		mount_ext2fs /dev/wd0s1 /mnt

	W celu zamontowania tylko do odczytu, nalezy dodac opcje -o ro.
