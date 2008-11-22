# make sure every subprocess has it's exit and that the main one
# hasn't
sub fun {
    unless ($pid = fork) {
        unless (fork) {
            use Tk;
            $MW = MainWindow->new;
            $hello = $MW->Button(
                -text    => 'Hello, world',
                -command => sub {exit;},
            );
            $hello->pack;
            MainLoop;
        }
        exit 0;
    }
    waitpid($pid, 0);
}

1;
