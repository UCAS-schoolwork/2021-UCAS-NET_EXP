#!/usr/bin/python

import os
import sys
import glob
import time

from mininet.topo import Topo
from mininet.net import Mininet
from mininet.link import TCLink
from mininet.cli import CLI

script_deps = [ 'ethtool' ]

def check_scripts():
    dir = os.path.abspath(os.path.dirname(sys.argv[0]))
    
    for fname in glob.glob(dir + '/' + 'scripts/*.sh'):
        if not os.access(fname, os.X_OK):
            print '%s should be set executable by using `chmod +x $script_name`' % (fname)
            sys.exit(1)

    for program in script_deps:
        found = False
        for path in os.environ['PATH'].split(os.pathsep):
            exe_file = os.path.join(path, program)
            if os.path.isfile(exe_file) and os.access(exe_file, os.X_OK):
                found = True
                break
        if not found:
            print '`%s` is required but missing. which could be installed via `apt` or `aptitude`' % (program)
            sys.exit(2)

# Mininet will assign an IP address for each interface of a node 
# automatically, but hub or switch does not need IP address.
def clearIP(n):
    for iface in n.intfList():
        n.cmd('ifconfig %s 0.0.0.0' % (iface))

class BroadcastTopo(Topo):
    def build(self):
        h1 = self.addHost('h1')
        h2 = self.addHost('h2')
        h3 = self.addHost('h3')
        b1 = self.addHost('b1')

        self.addLink(h1, b1, bw=20)
        self.addLink(h2, b1, bw=10)
        self.addLink(h3, b1, bw=10)

if __name__ == '__main__':
    check_scripts()

    topo = BroadcastTopo()
    net = Mininet(topo = topo, link = TCLink, controller = None) 

    h1, h2, h3, b1 = net.get('h1', 'h2', 'h3', 'b1')
    
    h1.cmd('ifconfig h1-eth0 10.0.0.1/8')
    h2.cmd('ifconfig h2-eth0 10.0.0.2/8')
    h3.cmd('ifconfig h3-eth0 10.0.0.3/8')
    
    clearIP(b1)

    for h in [ h1, h2, h3, b1 ]:
        h.cmd('./scripts/disable_offloading.sh')
        h.cmd('./scripts/disable_ipv6.sh')

    net.start()
    hub = './hub'
    print(b1.cmd(hub+' &'))
    
    mode = 2
    
    if mode==1:
        print('test h1')
        print(h1.cmd('ping -c 4 10.0.0.2'))
        print(h1.cmd('ping -c 4 10.0.0.3'))
        print('test h2')
        print(h2.cmd('ping -c 4 10.0.0.1'))
        print(h2.cmd('ping -c 4 10.0.0.3'))
        print('test h3')
        print(h3.cmd('ping -c 4 10.0.0.1'))
        print(h3.cmd('ping -c 4 10.0.0.2'))
    elif mode==2:
        print(h1.cmd('iperf -s > out1.txt &'))
        print(h2.cmd('iperf -c 10.0.0.1 -t 30 &'))
        print(h3.cmd('iperf -c 10.0.0.1 -t 30'))

        print('test h1 to h2 and h3:')
        print(h3.cmd('iperf > out3.txt -s &'))
        print(h2.cmd('iperf > out2.txt -s &'))
        print(h1.cmd('iperf -c 10.0.0.3 -t 30 & iperf -c 10.0.0.2 -t 30'))

    
    raw_input('done.')
    
    
    if mode==2:
        print('test h2 h3 to h1:')
        os.system('cat out1.txt')
        print('test h1 to h2 and h3:')
        os.system('cat out2.txt')
        os.system('cat out3.txt')
    b1.cmd('kill %'+hub)
    
    #CLI(net)
    net.stop()
