[1mfsck[m	narzedzie do sprawdzania poprawnosci i spojnosci filesystemu.

	Sposob uzycia:

	fsck -p [-f] [-m mode]
	fsck [-b block#] [-c level] [-l maxparallel] [-y] [-n] [-m mode]
		[filesystem] ...

	...ale w najprostszej i najczesciej spotykanej formie:

	fsck -y <filesystem>

	gdzie <filesystem> jest nazwa "surowego" urzadzenia, na ktorym
	znajduje sie system plikow, np. /dev/rfd0 dla dyskietki A:, lub
	/dev/rwd0s1 dla pierwszej partycji pierwszego dysku IDE.
