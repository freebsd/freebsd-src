Opcje: dysk_bios:kontroler(dysk,partycja)nazwa_kernela opcje
    dysk_bios    0, 1, ...
    kontroler    fd (dyskietka), wd (dysk IDE) lub sd (dysk SCSI)
    dysk         0, 1, ... (numer dysku w kontrolerze)
    partycja     a, c, e, f ... (wedlug nazewnictwa BSD)
    nazwa_kernela  nazwa pliku kernela, lub ? zeby dostac liste plikow
    opcje  -a (pytaj o rootdev) -C (cdrom) -c (userconf.) -D (podwojna konsola)
           -d (uruchom debugger) -g (gdb) -h (konsola szeregowa)
           -P (probkuj klawiature) -r (domyslny rootdev) -s (tryb single user)
           -v (verbose - szczegolowe komunikaty)
Np: 1:sd(0,a)mykernel  startuj `mykernel' z 1 dysku SCSI gdy jest tez 1 dysk
                       IDE, i jednoczesnie ustaw go jako domyslny dysk_bios,
                       kontroler, dysk i partycje
    -cv                startuj z parametrami domyslnymi, potem uruchom
                       konfiguracje param. sprzetowych (-c), i podawaj
                       szczegolowe komunikaty w trakcie bootowania (-v).
