#!./ruby -w

def func_c
  print "Function C\n"
  sleep 1
end

def func_b
  print "Function B\n"
  sleep 1
  func_c
end

def func_a
  print "Function A\n"
  sleep 1
  func_b
end

func_a
