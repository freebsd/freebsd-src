[1mmount[m	zamontuj system plikow

	Sposob uzycia:

	mount [-dfpruvw] [-o opcje] [-t [ufs|msdosfs|ext2fs] urzadzenie punkt

	N.p.:

	* zamontuj dyskietke DOS A: na katalogu /mnt:

		mount -t msdosfs /dev/fd0a /mnt

	* zamontuj pierwsza partycje DOS (na pierwszym dysku IDE) na /dos:

		mount -t msdosfs /dev/wd0s1 /dos

	* zamontuj partycje Linux na /mnt:

		mount -t ext2fs /dev/wd0s1 /mnt
