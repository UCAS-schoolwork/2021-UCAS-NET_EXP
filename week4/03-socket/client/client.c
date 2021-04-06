#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
 
#define SBUFSIZE 512
#define RBUFSIZE 2048

char* mystrcpy(char *t, const char* s)
{   // add '\0'
    while(*t++ = *s++)
        ;
    return t;
}
int main(int argc, char *argv[])
{
    if(argc!=2)
        return -1;

    int i=0;
    while(argv[1][i++]!='/')
        if(argv[1][i]=='\0') return -1;
    while(argv[1][i]=='/')
        i++;
    char *host = argv[1]+i;
    while(argv[1][i++]!=':')
        if(argv[1][i]=='\0') return -1;
    char *port = argv[1]+i;
    *(port-1) = '\0';
    while(argv[1][i++]!='/')
        if(argv[1][i]=='\0') return -1;
    char *url = argv[1]+i;
    *(url-1) = '\0';
    int iport = atoi(port);

    char recv_buf[RBUFSIZE];
    char send_buf[SBUFSIZE];

    // create socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        printf("create socket failed");
		return -1;
    }
    //printf("client socket created\n");
    struct sockaddr_in server;
    //memset(&server,0,sizeof(server));
    server.sin_addr.s_addr = inet_addr(host);
    server.sin_family = AF_INET;
    server.sin_port = htons(iport);
 
    // connect to server
    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("connect failed");
        return -1;
    }
     
    printf("client connected\n");
    const char header1[] = "GET /";
    const char header2[] = " HTTP/1.1\r\nConnection: close\r\nHost: ";
    
    strcpy(send_buf,header1);
    char* sp = mystrcpy(send_buf+sizeof(header1)-1,url);
    sp = mystrcpy(sp-1,header2);
    sp = mystrcpy(sp-1,host);
    *(sp-1) = '\r', *sp++ = '\n', *sp++ = '\r', *sp++ = '\n';

    // send 
    if (send(sock, send_buf, sp-send_buf, 0) < 0) {
        printf("send failed");
        goto FAILED;
    }
        
    // receive a reply from the server
    int len, recvsz = sizeof(recv_buf);
    sp = recv_buf;
    int isbig = 0;
    while((len = recv(sock, sp, recvsz, 0))>0){
        //printf("thislen:%d\n",len);
        sp+=len; 
        recvsz-=len;
        if(recvsz<sizeof(recv_buf)/2){
            isbig = 1;
            break;
        }
    }
    if (len < 0) {
        printf("recv failed");
        goto FAILED;
    }
    len = sizeof(recv_buf) - recvsz;
    //printf("len:%d message:%s\n",len,recv_buf);
    for(sp=recv_buf;sp-recv_buf<len&&*sp!=' ';sp++)
        ;
    if(!(*(sp+1)=='2'&&*(sp+2)=='0'&&*(sp+3)=='0'))
        goto FAIL;
    for(;;sp++){
        if(sp-recv_buf==len){
            *(sp-1) = '\0';
            printf("Pack len:%d\nMessage:%s\n",len,recv_buf);
            goto FAIL;
        }
        if(*sp=='\r'&&*(sp+1)=='\n'&&*(sp+2)=='\r'&&*(sp+3)=='\n')
            break;
    }
    sp += 4;
    long filesz = len-(sp-recv_buf);
    if(filesz<=0) {
FAIL:
        printf("get %s failed\n",url);
        goto FAILED;
    }
    const char recv_path[] = "./client/";
    strcpy(send_buf,recv_path);
    strcpy(send_buf+sizeof(recv_path)-1,url);
    printf("save to: %s\n",send_buf);
    FILE* fp = fopen(send_buf,"wb");
    len = filesz;
    if(fp==NULL){
        printf("fopen error\n");
        goto FAILED;
    } else {
        while(fwrite(sp,len,1,fp)>0){
            if(!isbig)
                goto SUCCESS;
            else if((len = recv(sock, recv_buf, sizeof(recv_buf), 0))>0){
                filesz += len;
                //printf("thislen:%d\n",len);
                sp = recv_buf;
            }
            else if(len==0)
                goto SUCCESS;
            else break;
        }
        printf("fwrite error\n");
        fclose(fp);
        goto FAILED;

SUCCESS: 
        fclose(fp);
        printf("HTTP get success. Filesz:%ld\n",filesz);
    }
    
FAILED:
    close(sock);
    return 0;
}
