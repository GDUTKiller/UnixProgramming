#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdint.h>

#define ISspace(x) isspace((int)(x))

#define SERVER_STRING  "Server: yjlhttpd/0.1.0\r\n"
#define error_die(msg) { perror(msg); exit(1); } 

void accept_request(void *);
void bad_request(int);
void cat(int, FILE *);
void cannot_execute(int);
void execute_cgi(int, const char *, const char *, const char *);
int  get_line(int, char *, int);
void headers(int, const char *);
void not_found(int);
void serve_file(int, const char *);
int  startup(u_short *);
void unimplemented(int);

/*
 * 这个函数在特定的端口上监听网络连接
 * 如果端口为0，则动态分配一个端口
 * 参数 指向变量port（端口）的指针
 * 返回 socket id
 */
int startup(u_short *port)
{
    int                 on    = 1;
    int                 httpd = 0;
    struct sockaddr_in  name;

    /*
     * 1.向内核请求一个socket
     */
    httpd = socket(PF_INET, SOCK_STREAM, 0);
    if (httpd == -1)
        error_die("socket");

    /*
     * 2.绑定地址到socket，地址即主机和端口
     */
    memset(&name, 0, sizeof(name));           /* 清空结构体 */ 
    name.sin_family = AF_INET;                /* 地址族 */
    name.sin_port = htons(*port);             /* 端口号 *port为0时自动分配 */
    name.sin_addr.s_addr = htonl(INADDR_ANY); /* 绑定到本地的默认IP地址上进行监听（多块网卡情况下） */
    if ((setsockopt(httpd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))) < 0) {
        error_die("setsockopt failed"); 
    }
    if (bind(httpd, (struct sockaddr *)&name, sizeof(name)) < 0) {
        error_die("bind");
    }
    if (*port == 0) {                         /* 如果是动态分配端口 */
        socklen_t namelen = sizeof(name);
        if (getsockname(httpd, (struct sockaddr *)&name, &namelen) == -1) {
            error_die("getsockname");
        }
        *port = ntohs(name.sin_port);
    }

    /*
     * 3.允许连接进来
     */
    if (listen(httpd, 5) < 0)
        error_die("listen");
    return httpd;
}

int main(void)
{
    struct sockaddr_in  client_name;

    int                 server_sock     = -1;
    int                 client_sock     = -1;
    u_short             port            = 4000;
    socklen_t           client_name_len = sizeof(client_name);
    pthread_t           newthread;

    server_sock = startup(&port);
    printf("httpd running on port %d\n", port);

    while (1)
    {
        client_sock = accept(server_sock,
                (struct sockaddr *)&client_name,
                &client_name_len);
        if (client_sock == -1)
            error_die("accept"); 
        /* 开启线程处理连接 */
        if (pthread_create(&newthread, NULL, (void*)accept_request, (void *)(intptr_t)cllient_sock) != 0)
            perror("pthread_create");
    }
    close(server_sock);

    return 0;
}
