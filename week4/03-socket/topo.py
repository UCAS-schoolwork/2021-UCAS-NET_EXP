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
# 4:CLI
mode = 1 
debug = 0
if(len(sys.argv)) >= 2:
    mode = int(sys.argv[1])
if(len(sys.argv)) == 3 and sys.argv[2]==1:
    debug = 1


topo = MyTopo()
net = Mininet(topo = topo, switch = OVSBridge, link = TCLink, controller = None)
net.start()

h1 = net.get('h1')

if debug:
    h1.cmd('wireshark &')
    time.sleep(6)

if(len(sys.argv)) == 2 and int(sys.argv[1])==4:
    CLI(net)
    net.stop()
    quit()

if mode==2:
    h1.cmd('rm -f ./client/server/test*')
    str1 = h1.cmd('python -m SimpleHTTPServer 80 &')
else:
    h1.cmd('rm -f wget*')
    h1.cmd('rm -f test*')
    path1 = './server/hserver'
    str1 = h1.cmd(path1 + ' 80 &')
#time.sleep(1)
print(str1)

h2 = net.get('h2')
if mode==1:
    str2 = h2.cmd('wget http://10.0.0.1:80/server/test1.html & wget http://10.0.0.1:80/server/test2.html & wget http://10.0.0.1:80/server/noexist.html')
else:
    str2 = h2.cmd('./client/hclient http://10.0.0.1:80/server/test1.html & ./client/hclient http://10.0.0.1:80/server/test2.html')# & ./client/hclient http://10.0.0.1:80/server/noexist.html')

print(str2)
#raw_input("done")
time.sleep(2)

if mode==2:
    print(h1.cmd('kill %python'))
else:
    print(h1.cmd('kill %' + path1))
    #print(h2.cmd('kill %wget'))

print("diff output:")
if mode==1:
    os.system('diff ./server/test1.html ./test1.html && diff ./server/test2.html ./test2.html')
else:
    os.system('diff ./server/test1.html ./client/server/test1.html && diff ./server/test2.html ./client/server/test2.html')

if debug:
    raw_input()

#CLI(net)
net.stop()
