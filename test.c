#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void extract_hostname_port_uri(const char *url, char *hostname, int *port, char *uri) {
    const char *start, *end, *port_start, *path_start;
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

int main() {
    char url[256];
    char hostname[256];
    char uri[256];
    int port;
while(1){
    printf("Enter URL: ");
    scanf("%s", url);

    extract_hostname_port_uri(url, hostname, &port, uri);

    printf("Hostname: %s\n", hostname);
    printf("Port: %d\n", port);
    printf("URI: %s\n", uri);
}
    

    return 0;
}
