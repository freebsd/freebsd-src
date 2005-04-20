sub forall {
  my ($code) = shift;
  my ($i) = $VI::StartLine-1;
  while (++$i <= $VI::StopLine) {
    $_ = $curscr->GetLine($i);
    VI::SetLine($VI::ScreenId, $i, $_) if(&$code);
  }
}

1;
