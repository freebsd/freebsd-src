[1mmount[m	zamontuj system plikow

	Sposob uzycia:

	mount [-dfpruvw] [-o opcje] [-t [ufs|msdos|ext2fs] urzadzenie punkt

	N.p.:

	* zamontuj dyskietke DOS A: na katalogu /mnt:

		mount -t msdos /dev/fd0a /mnt

	* zamontuj pierwsza partycje DOS (na pierwszym dysku IDE) na /dos:

		mount -t msdos /dev/wd0s1 /dos

	* zamontuj partycje Linux na /mnt:

		mount -t ext2fs /dev/wd0s1 /mnt
