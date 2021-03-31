from mininet.net import Mininet
from mininet.cli import CLI
from mininet.link import TCLink
from mininet.topo import Topo
from mininet.node import OVSBridge
import os
import time
import sys

class MyTopo(Topo):
    def build(self):
        h1 = self.addHost('h1')
        h2 = self.addHost('h2')
        s1 = self.addSwitch('s1')
        self.addLink(h1, s1, bw=0.1, delay='100ms')
        self.addLink(h2, s1)

# mode = 1:test server; 
# 2:test client 
# 3:test both
mode = 1 
if(len(sys.argv)) == 2:
    mode = int(sys.argv[1])

topo = MyTopo()
net = Mininet(topo = topo, switch = OVSBridge, link = TCLink, controller = None)
net.start()

h1 = net.get('h1')
if mode==2:
    str1 = h1.cmd('python -m SimpleHTTPServer 80 &')
else:
    path1 = './server/hserver'
    str1 = h1.cmd(path1 + ' 80 &')
print(str1)

h2 = net.get('h2')
if mode==1:
    h2.cmd('rm -f test*')
    str2 = h2.cmd('wget http://10.0.0.1:80/server/test1.html & wget http://10.0.0.1:80/server/test2.html & wget http://10.0.0.1:80/server/noexist.html')
else:
    h2.cmd('rm -f ./client/test*')
    str2 = h2.cmd('./client/hclient http://10.0.0.1:80/server/test1.html & ./client/hclient http://10.0.0.1:80/server/test2.html')

time.sleep(1)
print(str2)

if mode==2:
    print(h1.cmd('kill %python'))
else:
    print(h1.cmd('kill %' + path1))


# CLI(net)
# h1.waitOutput()
# raw_input()
net.stop()
