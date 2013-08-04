/*
 * Andrew ID: luningp
 * Name: Luning Pan
 * Proxy.c
 */
#include <stdio.h>
#include <string.h>
#include "csapp.h"

#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

static const char *user_agent = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *accept = "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
static const char *accept_encoding = "Accept-Encoding: gzip, deflate\r\n";
static const char *connection = "Connection: close\r\n";
static const char *proxy_connection = "Proxy-Connection: close\r\n";


//#define DEBUG_PRINT(msg) printf("%s\n", msg)

typedef struct _Request {
    char method[MAXLINE];
    char uri[MAXLINE]; 
    char version[MAXLINE];
    char hostname[MAXLINE];
    int port;
} Request, * PRequest;

void read_request(int connfd);
void check_request(PRequest pRequest);
void forward_request(PRequest pRequest);

void read_request(int connfd)
{
    char buf[MAXLINE];
    rio_t rio;
    struct Request request;
    
    /* Read request line and headers */
    Rio_readinitb(&rio, connfd);
    Rio_readlineb(&rio, buf, MAXLINE);
    sscanf(buf, "%s %s %s", request.method, request.uri, request.version);
    if (strcasecmp(request.method, "GET")) { 
       clienterror(connfd, request.method, "501", "Not Implemented",
                "Proxy does not implement this method");
        return;
    }

    //HTTP1.0 -> HTTP1.1

    return;
}

void check_request(PRequest pRequest)
{

        
    
    return;
}

int main(int argc, char **argv)
{
    //printf("%s%s%s", user_agent, accept, accept_encoding);
    int listenfd, connfd, port, clientlen;
    struct sockaddr_in clientaddr;

    /* Check command line args */
    if (argc != 2) {
        printf(stderr, "proxy usage: %s <port>\n", argv[0]);
        exit(1);
    }
    port = atoi(argv[1]);

    listenfd = Open_listenfd(port);
    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        doit(connfd);
        Close(connfd);
    }
    return 0;
}
