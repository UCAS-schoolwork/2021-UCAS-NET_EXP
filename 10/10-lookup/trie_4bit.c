#include <stdio.h>
#include <sys/time.h>    
#include <unistd.h>

#define MAXN 590000
#define MAXIP 700000

typedef struct{
    char b3[8];
    char b2[4];
    char b1[2];
    char port;
} Node;
int trie[MAXN][16];
Node port[MAXN];
int cnt, cntip;

int insert(unsigned ip, int mask, char p)
{
    int cur = 0, bit;
    if(MAXN<cnt+40) return 0;
    for(int i=3;i<mask;i+=4){
        bit = ip>>(31-i) & 15;
        if(trie[cur][bit]==0)
            trie[cur][bit] = ++cnt;
        cur = trie[cur][bit];
    }
    switch (mask%4)
    {
        case 3:
            bit = ip>>(32-mask)&0x7;
            if(port[cur].b3[bit]) return 0;
            else port[cur].b3[bit] = p+1;
            break;
        case 2:
            bit = ip>>(32-mask)&0x3;
            if(port[cur].b2[bit]) return 0;
            else port[cur].b2[bit] = p+1;
            break;
        case 1:
            bit = ip>>(32-mask)&0x1;
            if(port[cur].b1[bit]) return 0;
            else port[cur].b1[bit] = p+1;
            break;
        default:
            if(port[cur].port) return 0;
            else port[cur].port = p+1;
    }
    return 1;
}

char lookup(unsigned ip)
{
    int cur = 0;
    char p = 0;
    for(int i=3;i<32;i+=4) {
        int bit = ip>>(31-i) & 15;
        int next;
        int bit1 = bit;
        bit1 >>= 1;
        if(port[cur].b3[bit1])
            {p = port[cur].b3[bit1]; goto NEXT;}
        bit1 >>= 1;
        if(port[cur].b2[bit1])
            {p = port[cur].b2[bit1]; goto NEXT;}
        bit1 >>= 1;
        if(port[cur].b1[bit1])
            {p = port[cur].b1[bit1]; goto NEXT;}
    NEXT:
        next = trie[cur][bit];
        cur = next;
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
    int bit;
    for(int i=3;i<mask;i+=4){
        bit = ip>>(31-i) & 15;
        if((cur = trie[cur][bit])>0) {}
        else break;
    }
    switch (mask%4)
    {
        case 3:
            bit = ip>>(32-mask)&0x7;
            p = port[cur].b3[bit];
            break;
        case 2:
            bit = ip>>(32-mask)&0x3;
            p = port[cur].b2[bit];
            break;
        case 1:
            bit = ip>>(32-mask)&0x1;
            p = port[cur].b1[bit];
            break;
        default:
            p = port[cur].port;
    }
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
    int mem = cnt*(sizeof(Node)+sizeof(int)*16)/1000000;
    printf("Mem:%dMB Sum_val:%d\n",mem,ret);
    
    return 0;
}
