#include <stdio.h>
#include "csapp.h"
#include <string.h>
/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *connection_hdr = "Connection: close\r\n";
static const char *proxy_connection_hdr = "Proxy-Connection: close\r\n";
static const char *version_for_proxy = "HTTP/1.0";
void doit(int fd);
int parse_uri(char *uri, char *filename, char *cgiargs);
void read_requesthdrs(rio_t *rp, char *req_header_buf, char* host);
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg);
void parse_request_header(char* request_header, char* header, char* body,int maxlen);
void extract_hostname_port_uri(const char *url, char *hostname, int *port, char *uri);
void send_http_request(int clientfd, const char *method, const char *url, const char *version, const char *request_header);
size_t receive_http_response(int clientfd, char *outputBuffer, int bufferSize);
int main(int argc, char** argv)
{
    int listenfd,connfd;
    char hostname[MAXLINE/2],port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    if(argc!=2){
        fprintf(stderr, "usage: %s <port>\n",argv[0]);
        exit(1);
    }
    listenfd = Open_listenfd(argv[1]);
    if(connfd<0)
            exit(1);
    while(1){
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA*)&clientaddr, &clientlen);
        if(connfd<0)
            exit(1);
        Getnameinfo((SA*)&clientaddr, clientlen, hostname, MAXLINE, port,MAXLINE,0);

        doit(connfd);
        Close(connfd);
    }
    return 0;
}

void doit(int fd){
    char buf[MAXLINE], method[MAXLINE], url[MAXLINE], version[MAXLINE];
    char hostname[MAXLINE/2],port[MAXLINE],uri[MAXLINE];
    char req_header_buf[MAXLINE];
    char final_req_header_buf[MAXLINE];
    char output_buff[MAX_OBJECT_SIZE];
    rio_t rio;

    /* Read request line and headers */
    rio_readinitb(&rio, fd);
    if (!rio_readlineb(&rio, buf, MAXLINE))  //line:netp:doit:readrequest
        return;
    sscanf(buf, "%s %s %s", method, url, version);       //line:netp:doit:parserequest

    if (strcasecmp(method, "GET")) {                     //line:netp:doit:beginrequesterr
        clienterror(fd, method, "501", "Not Implemented",
                    "Tiny does not implement this method");
        return;
    }                                                    //line:netp:doit:endrequesterr

    int port_num;
    extract_hostname_port_uri(url,hostname,&port_num,uri);
    sprintf(port,"%d",port_num);

    char find_host=0;
    read_requesthdrs(&rio, req_header_buf,&find_host);              //line:netp:doit:readrequesthdrs
    final_req_header_buf[0]='\0';
    if(find_host==0){

        sprintf(final_req_header_buf,"Host:%s\r\n",hostname);
    }
    strcat(final_req_header_buf,user_agent_hdr);
    strcat(final_req_header_buf,connection_hdr);
    strcat(final_req_header_buf,proxy_connection_hdr);
    strcat(final_req_header_buf,req_header_buf);
    
    int clientfd = Open_clientfd(hostname,port);

    send_http_request(clientfd, method, uri, version_for_proxy, final_req_header_buf);
    size_t total_bytes_received=receive_http_response(clientfd,output_buff,MAX_OBJECT_SIZE);
    Rio_writen(fd, output_buff, total_bytes_received);

    Close(clientfd);
    return;
}
int parse_uri(char *uri, char *filename, char *cgiargs) 
{
    char *ptr;

    if (!strstr(uri, "cgi-bin")) {  /* Static content */ //line:netp:parseuri:isstatic
        strcpy(cgiargs, "");                             //line:netp:parseuri:clearcgi
        strcpy(filename, ".");                           //line:netp:parseuri:beginconvert1
        strcat(filename, uri);                           //line:netp:parseuri:endconvert1
        if (uri[strlen(uri)-1] == '/')                   //line:netp:parseuri:slashcheck
            strcat(filename, "home.html");               //line:netp:parseuri:appenddefault
        return 1;
    }
    else {  /* Dynamic content */                        //line:netp:parseuri:isdynamic
        ptr = index(uri, '?');                           //line:netp:parseuri:beginextract
        if (ptr) {
            strcpy(cgiargs, ptr+1);
            *ptr = '\0';
        }
        else 
            strcpy(cgiargs, "");                         //line:netp:parseuri:endextract
        strcpy(filename, ".");                           //line:netp:parseuri:beginconvert2
        strcat(filename, uri);                           //line:netp:parseuri:endconvert2
        return 0;
    }
}
void parse_request_header(char* request_header, char* header, char* body,int maxlen){
    char* pos =  strchr(request_header, ':');
    char* ptr = request_header;
    int cnt=0;
    while(ptr!=pos&&cnt<=maxlen){
        *header = *ptr;
        header++;
        cnt++;
        ptr++;
    }
    *header='\0';
    while(*ptr!='\0'&&cnt<=maxlen){
        *body = *ptr;
        body++;
        ptr++;
    }
    *body='\0';
    return;
}
void read_requesthdrs(rio_t *rp, char *req_header_buf, char* host)
{
    char buf[MAXLINE];
    req_header_buf[0]='\0';
    while(1) {          //line:netp:readhdrs:checkterm
        rio_readlineb(rp, buf, MAXLINE);
        if(!strcmp(buf, "\r\n"))break;
        char header[MAXLINE],body[MAXLINE];
        parse_request_header(buf,header,body,MAXLINE);
        if(!strcmp(header,"Host")){
            *host=1;
        }
        else if(!strcmp(header,"User-Agent") || !strcmp(header,"Connection") || !strcmp(header,"Proxy-Connection"))
        {
            continue;
        }
        strcat(req_header_buf, buf);
    }
    return;
}
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg) 
{
    char buf[MAXLINE], body[MAXBUF];

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-overflow"
    /* Build the HTTP response body */
    sprintf(body, "<html><title>Tiny Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);
#pragma GCC diagnostic pop

    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    rio_writen(fd, buf, strlen(buf));
    rio_writen(fd, body, strlen(body));
}
void send_http_request(int clientfd, const char *method, const char *url, const char *version, const char *request_header) {
    char buffer[MAXBUF];

    // 构建 HTTP 请求
    snprintf(buffer, sizeof(buffer), "%s %s %s\r\n%s\r\n", method, url, version, request_header);

    // 发送 HTTP 请求
    if (send(clientfd, buffer, strlen(buffer), 0) < 0) {
        perror("Error sending request");
        return;
    }

    printf("HTTP Request sent successfully\n");
}
size_t receive_http_response(int clientfd, char *outputBuffer, int bufferSize) {
    size_t total_bytes_received = 0;
    size_t bytes_received;
    while ((bytes_received = recv(clientfd, outputBuffer + total_bytes_received, bufferSize - total_bytes_received - 1)) > 0) {
        total_bytes_received += bytes_received;
        // 检查是否到达缓冲区的末尾
        if (total_bytes_received >= bufferSize - 1) {
            printf("Response buffer full\n");
            break;
        }
    }
    if (bytes_received < 0) {
        perror("Error receiving response");
    } else {
        //outputBuffer[total_bytes_received] = '\0'; // 确保字符串结束符
    }
    return total_bytes_received;
}
void extract_hostname_port_uri(const char *url, char *hostname, int *port, char *uri) {
    const char *start, *port_start, *path_start;
    start = strstr(url, "://");

    if (start != NULL) {
        start += 3; // 跳过 "://"
    } else {
        start = url;
    }

    port_start = strchr(start, ':'); // 查找端口号分隔符
    path_start = strchr(start, '/'); // 查找路径开始的地方

    if (path_start != NULL) {
        strcpy(uri, path_start); // 复制路径及其后面的部分作为 URI
    } else {
        strcpy(uri, "/"); // 如果没有路径，则 URI 为根路径
        path_start = url + strlen(url); // 设置路径开始为 URL 的末尾
    }

    if (port_start != NULL && port_start < path_start) {
        strncpy(hostname, start, port_start - start);
        hostname[port_start - start] = '\0';
        *port = atoi(port_start + 1);
    } else {
        strncpy(hostname, start, path_start - start);
        hostname[path_start - start] = '\0';
        if (strstr(url, "https://")) {
            *port = 443; // HTTPS 默认端口
        } else {
            *port = 80; // HTTP 默认端口
        }
    }
}