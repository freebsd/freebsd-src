[1mppp[m	obsluga protokolu PPP

	Sposob uzycia:

	ppp [-auto | -background | -direct | -dedicated | -ddial ] [system]

	W przypadku PicoBSD najczesciej bedzie to:

		ppp -background nazwa_polaczenia

	Nalezy przedtem uruchomic skrypt 'dialup' w celu poprawnej
	konfiguracji. Wowczas w celu dokonania polaczenia wystarczy:

		ppp -background dialup
