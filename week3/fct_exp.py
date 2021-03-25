from mininet.net import Mininet
from mininet.topo import Topo
from mininet.cli import CLI
from mininet.link import TCLink
from mininet.node import OVSBridge
import os

class MyTopo(Topo):
    def build(self,bw1=10):
        h1 = self.addHost('h1')
        h2 = self.addHost('h2')
        self.addLink(h1, h2, bw=bw1, delay='100ms')

def test(bw=10,sz=1):
    topo = MyTopo(bw1=bw)
    net = Mininet(topo = topo, switch = OVSBridge, link = TCLink, controller=None)

    net.start()
    h2 = net.get('h2')
    h2.cmd('python -m SimpleHTTPServer 80 &')

    h1 = net.get('h1')
    h2.cmd('dd if=/dev/zero of=1MB.dat bs='+str(sz)+'M count=1')
    str2 = h1.cmd('wget http://10.0.0.2/1MB.dat')
    os.system('sudo rm -f 1MB*')

    idx = str2.find(' in ') + 4
    time = atof(str2[idx:])

    #CLI(net)
    h2.cmd('kill %python')
    net.stop()

    return time

def atof(str):
    sum = 0.0
    isrem = 0
    for i in str:
        add = ord(i) - ord('0')
        if add>=0 and add<=9:
            if isrem==0:
                sum = sum*10 + add
            else:
                sum += add/isrem
                isrem *= 10
        elif i=='.' and isrem==0:
            isrem = 10.0
        else:
            break
    return sum


sz = [1,10,100]
bw = [10,50,100,500,1000]
szlist = [[0 for _ in range(len(bw))] for _ in range(len(sz))]
bw_imprv = [i/bw[0] for i in bw]
fct_imprv = []

for i in range(len(sz)):
    fct_imprv.append([])
    rep = 5
    for j in range(len(bw)):
        for _ in range(rep):
            t = test(bw[j],sz[i])
            szlist[i][j] += t
        szlist[i][j]/=rep*1.0
        fct_imprv[-1].append(szlist[i][0]/szlist[i][j])


print(szlist)
print(fct_imprv)

import matplotlib.pyplot as plt  
plt.figure(dpi=300)
for i in fct_imprv:   
    #plt.scatter(bw_imprv,i) 
    plt.plot(bw_imprv,i,label='filesz=%d MB'%sz[fct_imprv.index(i)],marker='o')

for i in fct_imprv: 
    for a, b in zip(bw_imprv, i):
        plt.text(1.03*a, 1.05*b, '(%d,%.2f)'%(a,b), ha='center', va='bottom', fontsize=8.5)

plt.plot([1,200],[1,200],label='Equal improvement in FCT and bandwidth')
plt.xscale('log')
plt.yscale('log')
plt.xlim(1,200)
plt.ylim(1,200)
plt.xticks([1,10,100])
plt.yticks([1,10,100])
plt.grid()
plt.xlabel('Relative Bandwidth Improvement(1=1Mbps)')
plt.ylabel('Relative FCT Improvement')
plt.legend()

#plt.show()
plt.savefig('plt.png',dpi=300)
