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

void accept_request(void *arg)
{
    int          client = (intptr_t)arg;
    int          cgi = 0;
    char         buf[1024];
    char         method[255];
    char         url[255];
    char         path[255];
    char        *query_string = NULL;
    size_t       numchars;
    size_t       i, j;
    struct stat  st;

    numchars = get_line(client, buf, sizeof(buf));
    i = 0; j = 0;
    while (!ISspace(buf[i]) && (i < sizeof(method) - 1)) {
        method[i] = buf[i];
        ++i;
    }
    j = i;
    method[i] = '\0';

    if (strcasecmp(method, "GET") && strcasecmp(method, "POST")) {
        unimplemented(client);
        return;
    }

    if (strcasecmp(method, "POST") == 0) {
        cgi = 1;
    }

    i = 0;
    while (ISspace(buf[j]) && (j < numchars)) {
        j++;
    }
    while (!ISspace(buf[j]) && (i < sizeof(url) - 1) && (j < numchars)) {
        url[i] = buf[j];
        i++; j++;
    }
    url[i] = '\0';

    if (strcasecmp(method, "GET") == 0) {
        query_string = url;
        while ((*query_string != '?') && (*query_string != '\0')) {
             query_string++;
        }
        if (*query_string == '?') {
            cgi = 1;
            *query_string = '\0';
            query_string++;
        }
    }

    sprintf(path, "htdocs%s", url);
    if (path[strlen(path) - 1] == '/') {
        strcat(path, "index.html");
    } 
    if (stat(path, &st) == -1) {
        while ((numchars > 0) && strcmp("\n", buf)) {
            numchars = get_line(client, buf, sizeof(buf));
        }
        not_found(client);
    } else {
        if ((st.st_mode & S_IFMT) == S_IFDIR) {
            strcat(path, "/index.html");
        }
        if ((st.st_mode & S_IXUSR)
            || (st.st_mode & S_IXGRP)
            || (st.st_mode & S_IXOTH)) {
            cgi = 1;
        }
        if (!cgi) {
            serve_file(client, path);
        } else {
            execute_cgi(client, path, method, query_string);
        }
    }
    close(client);
}

/**********************************************************************/
/* Get a line from a socket, whether the line ends in a newline,
 * carriage return, or a CRLF combination.  Terminates the string read
 * with a null character.  If no newline indicator is found before the
 * end of the buffer, the string is terminated with a null.  If any of
 * the above three line terminators is read, the last character of the
 * string will be a linefeed and the string will be terminated with a
 * null character.
 * Parameters: the socket descriptor
 *             the buffer to save the data in
 *             the size of the buffer
 * Returns: the number of bytes stored (excluding null) */
/**********************************************************************/
int get_line(int sock, char *buf, int size)
{
    int   i = 0, n;
    char  c = '\0';

    while ((i < size - 1) && (c != '\n')) {
        n = recv(sock, &c, 1, 0);
        /* DEBUG printf("%02X\n", c); */
        if (n > 0) {
            if (c == '\r') {
                n = recv(sock, &c, 1, MSG_PEEK);
                /* DEBUG printf("%02X\n", c); */
                if ((n > 0 ) && (c == '\n')) {
                    recv(sock , &c, 1, 0); 
                } else {
                    c = '\n'; 
                }
            }
            buf[i] = c;
            ++i;
        } else {
            c = '\n';
        }
    }
    buf[i] = '\0';

    return i;
}

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
