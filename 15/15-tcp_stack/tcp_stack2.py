#!/usr/bin/python2

import sys
import string
import socket
from time import sleep

data = string.digits + string.lowercase + string.uppercase

rfname = './client-input.dat'
wfname = './server-output.dat'

def server(port):
    s = socket.socket()
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    
    s.bind(('0.0.0.0', int(port)))
    s.listen(3)
    
    cs, addr = s.accept()
    print addr

    with open(wfname,'wb') as f:
        while True:
            data = cs.recv(1400)
            if data:
                f.write(data)
            else:
                break
    
    s.close()


def client(ip, port):
    s = socket.socket()
    s.connect((ip, int(port)))
    
    with open(rfname,'rb') as f:
        while True:
            sleep(0.002)
            data = f.read(1400)
            if data:
                s.send(data)
            else:
                break
    
    s.close()

if __name__ == '__main__':
    if sys.argv[1] == 'server':
        server(sys.argv[2])
    elif sys.argv[1] == 'client':
        client(sys.argv[2], sys.argv[3])
