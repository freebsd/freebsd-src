#!./ruby -w

def func_c
  print "Function C\n"
  i = 0
  while i < 300000
     i = i + 1
     j = i + 1
  end
end

def func_b
  print "Function B\n"
  i = 0
  while i < 200000
     i = i + 1
     j = i + 1
  end
  func_c
end

def func_a
  print "Function A\n"
  i = 0
  while i < 100000
     i = i + 1
     j = i + 1
  end
  func_b
end

func_a
