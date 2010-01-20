sub wc {
  my $words;
  $i = $VI::StartLine;
  while ($i <= $VI::StopLine) {
    $_ = $curscr->GetLine($i++);
    $words+=split;
  }
  $curscr->Msg("$words words");
}

1;
