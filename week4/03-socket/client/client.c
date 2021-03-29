#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
 
#define SBUFSIZE 128
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
    printf("client socket created\n");
    struct sockaddr_in server;
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
    int len = recv(sock, recv_buf, sizeof(recv_buf), 0);
    if (len < 0) {
        printf("recv failed");
        goto FAILED;
    }
    for(sp=recv_buf;;sp++){
        if(sp-recv_buf==len)
            goto FAILED;
        if(*sp=='\r'&&*(sp+1)=='\n'&&*(sp+2)=='\r'&&*(sp+3)=='\n')
            break;
    }
    sp += 4;
    long filesz = len-(sp-recv_buf);
    
    const char recv_path[] = "./client/";
    strcpy(send_buf,recv_path);
    strcpy(send_buf+sizeof(recv_path)-1,url);
    printf("sendbuf %s\n",send_buf);
    FILE* fp = fopen(send_buf,"wb");
    if(fp==NULL){
        printf("fopen error\n");
        goto FAILED;
    } else {
        if(fwrite(sp,filesz,1,fp)==0){
            printf("fwrite error\n");
            fclose(fp);
            goto FAILED;
        }
        fclose(fp);
        printf("HTTP get success. Filesz:%ld\n",filesz);
    }
    
FAILED:
    close(sock);
    return 0;
}