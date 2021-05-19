#include <stdio.h>
#include <sys/time.h>    
#include <unistd.h>

#define MAXN 1000000
#define MAXIP 700000

typedef struct{
    char port;
    char odd[2];
} Node;
int trie[MAXN][4];
Node port[MAXN];
int cnt, cntip;

int insert(unsigned ip, int mask, char p)
{
    int cur = 0, bit;
    if(MAXN<cnt+40) return 0;
    for(int i=1;i<mask;i+=2){
        bit = ip>>(31-i) & 3;
        if(trie[cur][bit]==0)
            trie[cur][bit] = ++cnt;
        cur = trie[cur][bit];
    }
    if(mask&1){
        bit = ip>>(32-mask)&1;
        if(port[cur].odd[bit]) return 0;
        else port[cur].odd[bit] = p+1;
    }
    else{
        if(port[cur].port) return 0;
        else port[cur].port = p+1;
    }
    return 1;
}

char lookup(unsigned ip)
{
    int cur = 0;
    char p = 0;
    for(int i=1;i<32;i+=2) {
        int bit = ip>>(31-i) & 3;
        int bit1 = bit>>1;
        if(port[cur].odd[bit1])
            p = port[cur].odd[bit1];
        cur = trie[cur][bit];
        if(cur==0)
            break;
        if(port[cur].port)
            p = port[cur].port; 
    }
    return p-1;
}

int verify(unsigned ip, int mask, char ps)
{
    int cur = 0;
    char p = 0;
    for(int i=1;i<mask;i+=2){
        int bit = ip>>(31-i) & 3;
        if((cur = trie[cur][bit])>0) {}
        else break;
    }
    if(mask&1)
        p = port[cur].odd[ip>>(32-mask)&1];
    else 
        p = port[cur].port;
    return p==ps+1;
}



unsigned ipbuf[MAXIP]; // temp for test
char tmpmask[MAXIP];
char tmpport[MAXIP];
void readip()
{
    const char *f = "./forwarding-table.txt";
    FILE *fp = fopen(f,"r");
    if(fp==NULL) return;
    int t[4], m, p;
    while(fscanf(fp,"%d.%d.%d.%d %d %d",t,t+1,t+2,t+3,&m,&p)==6)
    {
        unsigned ip = (((((t[0]<<8)+t[1])<<8)+t[2])<<8)+t[3];
        if(!insert(ip,m,p)){
            printf("insert fail\n");
            break;
        }  
        ipbuf[cntip++] = ip;
        tmpmask[cntip-1] = m;
        tmpport[cntip-1] = p;
    }
    for(int i=0;i<cntip;i++){
        if(!verify(ipbuf[i],tmpmask[i],tmpport[i])){
            printf("fail at %d\n",i);
            break;
        }
    }
}

int test()
{
    int portsum = 0;
    for(int i=0;i<cntip;i++){
        portsum += lookup(ipbuf[i]);
    }
    return portsum;
}

int main()
{
    struct timeval tv1,tv2;
    readip();
    printf("Entries:%d Nodes: %d\n",cntip,cnt);
    
    gettimeofday(&tv1,NULL);
    int ret = test();
    gettimeofday(&tv2,NULL);
    unsigned long us = (tv2.tv_sec-tv1.tv_sec)*1000000+(tv2.tv_usec-tv1.tv_usec);
    printf("Time:%lu us Avg:%lu ns\n",us,us*1000/cntip);
    int mem = cnt*(sizeof(Node)+sizeof(int)*4)/1000000;
    printf("Mem:%dMB Sum_val:%d\n",mem,ret);
    
    return 0;
}
