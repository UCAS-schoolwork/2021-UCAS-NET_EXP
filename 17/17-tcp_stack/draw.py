import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt  

fn = 'cwnd.dat'
x = []
y = []
with open(fn) as f:
    nline=0
    for line in f:
        x.append(int(line.split(' ')[0])/1000000.0)
        y.append(int(line.split(' ')[1]))
        nline += 1

plt.figure(dpi=300)
plt.plot(x,y)
plt.grid()
plt.xlabel('time(s)')
plt.ylabel('cwnd(MSS)')
plt.legend()
plt.savefig('plt.png')