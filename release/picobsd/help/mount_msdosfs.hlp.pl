[1mmount_msdosfs[m	zamontuj system plikow MS-DOS

	Sposob uzycia:

	mount_msdosfs [-o opcje] [-u user] [-g grupa] [-m maska] bdev dir

	N.p. zeby zamontowac partycje C: z dysku IDE na /doc

		mount_msdosfs /dev/wd0s1 /dos

	W celu zamontowania pierwszej partycji extended"

		mount_msdosfs /dev/wd0s5 /mnt
