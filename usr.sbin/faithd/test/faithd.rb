# faithd, ruby version.  requires v6-enabled ruby.
#
# highly experimental (not working right at all) and very limited
# functionality.
#
# $Id: faithd.rb,v 1.1.1.1 1999/08/08 23:29:31 itojun Exp $
# $FreeBSD: src/usr.sbin/faithd/test/faithd.rb,v 1.1 2000/01/27 09:28:38 shin Exp $

require "socket"
require "thread"

# XXX should be derived from system headers
IPPROTO_IPV6 = 41
IPV6_FAITH = 29
DEBUG = true
DEBUG_LOOPBACK = true

# TODO: OOB data handling
def tcpcopy(s1, s2, m)
  STDERR.print "tcpcopy #{s1} #{s2}\n" if DEBUG
  buf = ""
  while TRUE
    begin
      buf = s1.sysread(100)
      s2.syswrite(buf)
    rescue EOFError
      break
    rescue IOError
      break
    end
  end
  STDERR.print "tcpcopy #{s1} #{s2} finished\n" if DEBUG
  s1.shutdown(0)
  s2.shutdown(1)
end

def relay_ftp_passiveconn(s6, s4, dport6, dport4)
  Thread.start do
    d6 = TCPserver.open("::", dport6).accept
    d4 = TCPsocket.open(s4.getpeer[3], dport4)
    t = []
    t[0] = Thread.start do
      tcpcopy(d6, d4)
    end
    t[1] = Thread.start do
      tcpcopy(d4, d6)
    end
    for i in t
      i.join
    end
    d4.close
    d6.close
  end
end

def ftp_parse_2428(line)
  if (line[0] != line[line.length - 1])
    return nil
  end
  t = line.split(line[0 .. 0])	# as string
  if (t.size != 4 || t[1] !~ /^[12]$/ || t[3] !~ /^\d+$/)
    return nil
  end
  return t[1 .. 3]
end

def relay_ftp_command(s6, s4, state)
  STDERR.print "relay_ftp_command start\n" if DEBUG
  while TRUE
    begin
      STDERR.print "s6.gets\n" if DEBUG
      line = s6.gets
      STDERR.print "line is #{line}\n" if DEBUG
      if line == nil
	return nil
      end

      # translate then copy
      STDERR.print "line is #{line}\n" if DEBUG
      if (line =~ /^EPSV\r\n/i)
	STDERR.print "EPSV -> PASV\n" if DEBUG
	line = "PASV\n"
	state = "EPSV"
      elsif (line =~ /^EPRT\s+(.+)\r\n/i)
	t = ftp_parse_2428($1)
	if t == nil
	  s6.puts "501 illegal parameter to EPRT\r\n"
	  next
	end

	# some tricks should be here
	s6.puts "501 illegal parameter to EPRT\r\n"
	next
      end
      STDERR.print "fail: send #{line} as is\n" if DEBUG
      s4.puts(line)
      break
    rescue EOFError
      return nil
    rescue IOError
      return nil
    end
  end
  STDERR.print "relay_ftp_command finish\n" if DEBUG
  return state
end

def relay_ftp_status(s4, s6, state)
  STDERR.print "relay_ftp_status start\n" if DEBUG
  while TRUE
    begin
      line = s4.gets
      if line == nil
	return nil
      end

      # translate then copy
      s6.puts(line)

      next if line =~ /^\d\d\d-/
      next if line !~ /^\d/

      # special post-processing
      case line
      when /^221 /	# result to QUIT
	s4.shutdown(0)
	s6.shutdown(1)
      end

      break if (line =~ /^\d\d\d /)
    rescue EOFError
      return nil
    rescue IOError
      return nil
    end
  end
  STDERR.print "relay_ftp_status finish\n" if DEBUG
  return state
end

def relay_ftp(sock, name)
  STDERR.print "relay_ftp(#{sock}, #{name})\n" if DEBUG
  while TRUE
    STDERR.print "relay_ftp(#{sock}, #{name}) accepting\n" if DEBUG
    s = sock.accept
    STDERR.print "relay_ftp(#{sock}, #{name}) accepted #{s}\n" if DEBUG
    Thread.start do
      threads = []
      STDERR.print "accepted #{s} -> #{Thread.current}\n" if DEBUG
      s6 = s
      dest6 = s.addr[3]
      if !DEBUG_LOOPBACK
	t = s.getsockname.unpack("x8 x12 C4")
	dest4 = "#{t[0]}.#{t[1]}.#{t[2]}.#{t[3]}"
	port4 = s.addr[1]
      else
	dest4 = "127.0.0.1"
	port4 = "ftp"
      end
      if DEBUG
	STDERR.print "IPv6 dest: #{dest6}  IPv4 dest: #{dest4}\n" if DEBUG
      end
      STDERR.print "connect to #{dest4} #{port4}\n" if DEBUG
      s4 = TCPsocket.open(dest4, port4)
      STDERR.print "connected to #{dest4} #{port4}, #{s4.addr[1]}\n" if DEBUG
      state = 0
      while TRUE
	# translate status line
	state = relay_ftp_status(s4, s6, state)
	break if state == nil
	# translate command line
	state = relay_ftp_command(s6, s4, state)
	break if state == nil
      end
      STDERR.print "relay_ftp(#{sock}, #{name}) closing s4\n" if DEBUG
      s4.close
      STDERR.print "relay_ftp(#{sock}, #{name}) closing s6\n" if DEBUG
      s6.close
      STDERR.print "relay_ftp(#{sock}, #{name}) done\n" if DEBUG
    end
  end
  STDERR.print "relay_ftp(#{sock}, #{name}) finished\n" if DEBUG
end

def relay_tcp(sock, name)
  STDERR.print "relay_tcp(#{sock}, #{name})\n" if DEBUG
  while TRUE
    STDERR.print "relay_tcp(#{sock}, #{name}) accepting\n" if DEBUG
    s = sock.accept
    STDERR.print "relay_tcp(#{sock}, #{name}) accepted #{s}\n" if DEBUG
    Thread.start do
      threads = []
      STDERR.print "accepted #{s} -> #{Thread.current}\n" if DEBUG
      s6 = s
      dest6 = s.addr[3]
      if !DEBUG_LOOPBACK
	t = s.getsockname.unpack("x8 x12 C4")
	dest4 = "#{t[0]}.#{t[1]}.#{t[2]}.#{t[3]}"
	port4 = s.addr[1]
      else
	dest4 = "127.0.0.1"
	port4 = "telnet"
      end
      if DEBUG
	STDERR.print "IPv6 dest: #{dest6}  IPv4 dest: #{dest4}\n" if DEBUG
      end
      STDERR.print "connect to #{dest4} #{port4}\n" if DEBUG
      s4 = TCPsocket.open(dest4, port4)
      STDERR.print "connected to #{dest4} #{port4}, #{s4.addr[1]}\n" if DEBUG
      [0, 1].each do |i|
	threads[i] = Thread.start do
	  if (i == 0)
	    tcpcopy(s6, s4)
	  else
	    tcpcopy(s4, s6)
	  end
	end
      end
      STDERR.print "relay_tcp(#{sock}, #{name}) wait\n" if DEBUG
      for i in threads
	STDERR.print "relay_tcp(#{sock}, #{name}) wait #{i}\n" if DEBUG
	i.join
	STDERR.print "relay_tcp(#{sock}, #{name}) wait #{i} done\n" if DEBUG
      end
      STDERR.print "relay_tcp(#{sock}, #{name}) closing s4\n" if DEBUG
      s4.close
      STDERR.print "relay_tcp(#{sock}, #{name}) closing s6\n" if DEBUG
      s6.close
      STDERR.print "relay_tcp(#{sock}, #{name}) done\n" if DEBUG
    end
  end
  STDERR.print "relay_tcp(#{sock}, #{name}) finished\n" if DEBUG
end

def usage()
  STDERR.print "usage: #{$0} [-f] port...\n"
end

#------------------------------------------------------------

$mode = "tcp"

while ARGV[0] =~ /^-/ do
  case ARGV[0]
  when /^-f/
    $mode = "ftp"
  else
    usage()
    exit 0
  end
  ARGV.shift
end

if ARGV.length == 0
  usage()
  exit 1
end

ftpport = Socket.getservbyname("ftp")

res = []
for port in ARGV
  t = Socket.getaddrinfo(nil, port, Socket::PF_INET6, Socket::SOCK_STREAM,
	nil, Socket::AI_PASSIVE)
  if (t.size <= 0)
    STDERR.print "FATAL: getaddrinfo failed (port=#{port})\n"
    exit 1
  end
  res += t
end

sockpool = []
names = []
listenthreads = []

res.each do |i|
  s = TCPserver.new(i[3], i[1])
  n = Socket.getnameinfo(s.getsockname, Socket::NI_NUMERICHOST|Socket::NI_NUMERICSERV).join(" port ")
  if i[6] == IPPROTO_IPV6
    s.setsockopt(i[6], IPV6_FAITH, 1)
  end
  s.setsockopt(Socket::SOL_SOCKET, Socket::SO_REUSEADDR, 1)
  sockpool.push s
  names.push n
end

if DEBUG
  (0 .. sockpool.size - 1).each do |i|
    STDERR.print "listen[#{i}]: #{sockpool[i]} #{names[i]}\n" if DEBUG
  end
end

(0 .. sockpool.size - 1).each do |i|
  listenthreads[i] = Thread.start do
    if DEBUG
      STDERR.print "listen[#{i}]: thread #{Thread.current}\n" if DEBUG
    end
    STDERR.print "listen[#{i}]: thread #{Thread.current}\n" if DEBUG
    case $mode
    when "tcp"
      relay_tcp(sockpool[i], names[i])
    when "ftp"
      relay_ftp(sockpool[i], names[i])
    end
  end
end

for i in listenthreads
  i.join
end

exit 0
