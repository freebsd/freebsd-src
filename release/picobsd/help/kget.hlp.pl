[1mkget[m	Utworz liste konfiguracji jadra

	Ten program pozwala uzyskac parametry sterownikow urzadzen,
	ktore byc moze zostaly zmienione na etapie uruchamiania systemu z
	flaga '-c', pozwala rowniez zachowac te liste w specjalnym formacie
	do pliku /kernel.config, zeby zostala uzyta jako dane konfiguracyjne
	przy nastepnym starcie systemu.

	Sposob uzycia:

		kget [-incore|nazwa_jadra] [vanilla]

	E.g.:	Utworz liste parametrow dzialajacego jadra:

		kget -incore

		Utworz liste roznic w parametrach w stosunku do listy
		zawartej w pliku /stand/vanilla:

		kget -incore /stand/vanilla

		(Wynik tego polecenia mozna przekierowac wprost do pliku
		/kernel.config na dyskietce)
