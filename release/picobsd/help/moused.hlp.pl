[1mmoused[m	mouse daemon

	Sposob uzycia:

     moused [-3DPRcdfs] [-F rate] [-r resolution] [-S baudrate] [-C threshold]
     [-m N=M] [-z target] [-t mousetype] -p port

	Najczestsze opcje to:

     -3      emulacja trzeciego (srodkowego) przycisku na dwu-klawiszowych
             myszach.
     -p port nazwa portu: /dev/cuaa0 == COM1:, psm0 == gniazdo PS/2
     -t typ
	     microsoft	      Microsoft (2 przyciski) mysz szeregowa.
	     intellimouse     Microsoft IntelliMouse, Genius Net- Mouse,
			      ASCII Mie Mouse, Logitech MouseMan+, FirstMouse+
	     mousesystems     MouseSystems
	     mmseries	      MM Series
	     logitech	      Logitech. Ten protokol jest dla starszych typow
			      myszy - dla nowszych trzeba uzywac mouseman lub
			      intellimouse
	     mouseman	      Logitech MouseMan i TrackMan
	     glidepoint       ALPS GlidePoint
	     thinkingmouse    Kensington ThinkingMouse protocol.
	     mmhittab	      Hitachi tablet

Mozna wlaczyc wyswietlanie wskaznika myszy przez:

	   vidcontrol -m on
