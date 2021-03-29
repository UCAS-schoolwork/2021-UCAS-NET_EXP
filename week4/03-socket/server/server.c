#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
 
#define MAXTHREADS 4
#define SBUFSIZE 2048
#define RBUFSIZE 512

int threads = MAXTHREADS;
const char ok[] = "HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: ";
const char notfound[] = "HTTP/1.1 404 Not Found\r\n\r\n";
const char badrequest[] = "HTTP/1.1 400 Bad Request\r\n\r\n";

char* myitoa(char *s, unsigned n)
{   // no '\0'
    int i = 0, j = 0, c;
    do{
        s[i++] = n%10+'0';
    } while((n/=10)>0);
    char * t = s + i--;
    for(;j<i;j++,i--)
        c = s[i], s[i] = s[j], s[j] = c;
    return t;
}

void * handler(void * vargp){
    int fd = *(int*)vargp;
    pthread_t tid = pthread_self();
    pthread_detach(tid);
    free(vargp);
    char recv_buf[RBUFSIZE];
    char send_buf[SBUFSIZE];
    printf("connection accepted, tid is %ld\n",tid);
    
	int msg_len = 0;
    while ((msg_len = recv(fd, recv_buf, sizeof(recv_buf), 0)) > 0) {
        char* sp = recv_buf;
        if(sp[0]=='G'&&sp[1]=='E'&&sp[2]=='T'&&sp[3]==' '){
            sp += 4;
            while(*sp=='/')
                sp++;
            char *url = sp;
            while(*sp!=' '){
                if(sp++>=recv_buf+sizeof(recv_buf))
                    goto FAILED;
            }
            *sp = '\0';
            printf("(%ld)url:%s\n",tid,url);
            FILE* fp = fopen(url,"rb");
            if(fp==NULL)
                goto NOTFOUND;
                          
            else {
                fseek(fp,0,SEEK_END);
                long nCount = ftell(fp);
                printf("(%ld)filesz:%ld\n",tid,nCount);
                fseek(fp,0,SEEK_SET);
                if(nCount>=sizeof(send_buf)-sizeof(ok)+1-4)
                    goto FAILED;  
                strcpy(send_buf,ok);
                sp = myitoa(send_buf+sizeof(ok)-1,(unsigned)nCount);
                *sp++ = '\r', *sp++ = '\n', *sp++ = '\r', *sp++ = '\n';
                int headerlen = sp-send_buf;
                if( fread(sp,1,sizeof(send_buf)-headerlen,fp)>0 ){
                    write(fd, send_buf, nCount+headerlen);
                    goto SHUTDOWN;
                }
                else 
                    printf("file read error\n");
                fclose(fp);
            }
        }
        else 
            goto FAILED;
    }
    if(msg_len ==0){
        printf("Connect end.\n");
        goto SHUTDOWN;
    }
    else
        goto FAILED;
NOTFOUND:
    printf("NOTFOUND\n");
    write(fd,notfound,sizeof(notfound)-1);
    goto SHUTDOWN;
FAILED:
    printf("BADREQUEST\n");
    write(fd,badrequest,sizeof(badrequest)-1);
SHUTDOWN:
    shutdown(fd, SHUT_WR); 
    recv(fd, recv_buf,sizeof(recv_buf), 0); 
    close(fd);
    threads++;
    return NULL;
}

int main(int argc, const char *argv[])
{
    int port = 80;
    if(argc==2){
        port = atoi(argv[1]);
    }
    // create socket
    int fd;
    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("create socket failed");
		return -1;
    }
    printf("socket created\n");
    int on = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on) );
    // prepare the sockaddr_in structure
    struct sockaddr_in skaddr,clientaddr;
    skaddr.sin_family = AF_INET;
    skaddr.sin_addr.s_addr = INADDR_ANY;
    skaddr.sin_port = htons(port);
     
    // bind
    if (bind(fd,(struct sockaddr *)&skaddr, sizeof(skaddr)) < 0) {
        perror("bind failed");
        return -1;
    }
    printf("bind done\n");
     
    // listen
    listen(fd, MAXTHREADS+1);
    printf("waiting for incoming connections...\n");

    int clientlen = sizeof(struct sockaddr_in);
    int *clientfd;
    pthread_t tid;
    while(1){
        if(threads>0){
            int cfd = accept(fd, (struct sockaddr*)&clientaddr, (socklen_t*)&clientlen);
            if(cfd<0)
                continue;
            clientfd = (int*)malloc(sizeof(int)); //free by its child thread.
            *clientfd = cfd;
            threads--;
            if(pthread_create(&tid,NULL,handler,clientfd)!=0)
                perror("thread create error\n"); 
        }
    }
    return 0;
}
