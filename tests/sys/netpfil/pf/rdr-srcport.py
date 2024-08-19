#
# A helper script which accepts TCP connections and writes the remote port
# number to the stream.
#

import socket

def main():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(('0.0.0.0', 8888))
    s.listen(5)

    while True:
        cs, addr = s.accept()
        cs.sendall(str(addr[1]).encode())
        cs.close()

if __name__ == '__main__':
    main()
