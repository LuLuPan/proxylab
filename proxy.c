/*******************
 * Andrew ID: luningp
 * Name: Luning Pan
 * Proxy.c
 *******************
 */
#include <stdio.h>
#include <string.h>
#include "csapp.h"

#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

static const char *user_agent = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *accept_msg = "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
static const char *accept_encoding = "Accept-Encoding: gzip, deflate\r\n";
static const char *connection = "Connection: close\r\n";
static const char *proxy_connection = "Proxy-Connection: close\r\n";

#define DEBUG  1

#if DEBUG
#define DEBUG_PRINT(msg) printf("%s", msg)
#else
#define DEBUG_PRINT(msg)
#endif

typedef struct _Request {
    char method[MAXLINE];
    char uri[MAXLINE]; 
    char version[MAXLINE];
    char hostname[MAXLINE];
    int port;
} Request, * PRequest;

int read_request(int connfd, char *req_buf, char *hostname, int *port);
void check_request(PRequest pRequest);
int forward_request(rio_t *rio, char *req_buf, int clientfd);
int forward_response(rio_t *rio_server, char *resp_buf, int connfd);


/* parse positive number in the string */
int parse_num(char *str)
{
    char *ptr = NULL;
    char num[64];
    char *num_ptr = num;
    ptr = str;
    while(((*ptr < '0') || (*ptr > '9')) && (*ptr != '\r'))
        ptr++;
    
    if(*ptr >= '0' && *ptr <= '9')
    {
        /*find num*/
         while(*ptr >= '0' && *ptr <= '9')
         {
            *num_ptr = *ptr;
            num_ptr++;
            ptr++;
         }

         return atoi(num);
    }
    else
        return -1;
}


char* find_string(char *buf, char *target)
{
    if(buf == NULL || target == NULL)
        return 0;
    return strstr(buf, target);
}

int parse_reqhdr(char *buf, char *hostname, int *port)
{
     char *ptr = buf;
     char port_str[6];//max port 65535
     char *port_ptr = port_str;
     
     if(buf == NULL || hostname == NULL)
     {
        printf("parse_reqhdr: Invalid params!\n");
        return -1;
     }

     while(*ptr != '\r') 
     {
        if(*ptr == ' ')
        {
            ptr++;
            break;
        }
        ptr++;
     }

     while(*ptr != '\r') 
     {
        if(*ptr == ':')
        {
            ptr++;
            break;
        }

        *hostname = *ptr;
        hostname++;
        ptr++;
     }
     *hostname = '\0';
     
     if(*ptr == ':')
     {
         while(*ptr >= '0' && *ptr <= '9')
         {
            *port_ptr = *ptr;
            port_ptr++;
            ptr++;
         }

         *port = atoi(port_str);
     }
     else
     {
        *port = 80;
     }
        
     return 0;
}

int read_request(int connfd, char *req_buf, char *hostname, int *port)
{
    int is_static;
    struct stat sbuf;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char req_tmp[MAXLINE];
    //char filename[MAXLINE], cgiargs[MAXLINE];
    rio_t rio;
    int result = 0;
    //struct Request request;
  
    /* Read request line */
    Rio_readinitb(&rio, connfd);
    if(rio_readlineb(&rio, buf, MAXLINE) < 0)
    {
        printf("rio_readlineb fail!\n");
        return -1;
    }
    sscanf(buf, "%s %s %s", method, uri, version);
    /* HTTP1.1 -> HTTP1.0 */
    sprintf(req_buf, "%s %s HTTP/1.0\r\n", method, uri);
    //DEBUG_PRINT(req_buf);
    memset(buf, 0, MAXLINE);
    /* Read request header*/
    if(rio_readlineb(&rio, buf, MAXLINE) < 0)
    {
        printf("rio_readlineb fail!\n");
        return -1;
    }
    //DEBUG_PRINT(buf);

    while(1)
    {
        /* empty line? */
        if(strcmp(buf, "\r\n") == 0)
        {
            //DEBUG_PRINT("Empty line!\n");
            break;
        }
        if(find_string(buf, "Host:"))
        {
            parse_reqhdr(buf, hostname, port);
            sprintf(req_tmp, "Host: %s\r\n", hostname);
            strcat(req_buf, req_tmp);
            //DEBUG_PRINT(req_buf);
        }
        else if(find_string(buf, "User-Agent:"))
        {
            sprintf(req_tmp, "%s", user_agent);
            strcat(req_buf, req_tmp);
            //DEBUG_PRINT(req_buf);
        }
        else if(find_string(buf, "Accept:"))
        {
            sprintf(req_tmp, "%s", accept_msg);
            strcat(req_buf, req_tmp);
            //DEBUG_PRINT(req_buf);
        }
        else if(find_string(buf, "Accept-Encoding:"))
        {
            sprintf(req_tmp, "%s", accept_encoding);
            strcat(req_buf, req_tmp);
            //DEBUG_PRINT(req_buf);
        }
        else if(find_string(buf, "Connection:"))
        {
            sprintf(req_tmp, "%s", connection);
            strcat(req_buf, req_tmp);
            //DEBUG_PRINT(req_buf);
        }
        else if(find_string(buf, "Proxy-Connection:"))
        {
            sprintf(req_tmp, "%s", proxy_connection);
            strcat(req_buf, req_tmp);
            //DEBUG_PRINT(req_buf);
        }
                
        memset(buf, 0, sizeof(buf));
        if(rio_readlineb(&rio, buf, MAXLINE) < 0)
        {
            fprintf(stderr, "rio_readlineb read request error\n");
            return -1;
        }
        //DEBUG_PRINT(buf);
        
    }

    //Todo: if without above request details, always send them here


    //request empty line separater
    strcat(req_buf, "\r\n");
    //DEBUG_PRINT(req_buf);
    return 0;
}

int forward_request(rio_t *rio, char *req_buf, int clientfd)
{
    rio_readinitb(rio, clientfd);

    /* send req msg to server*/
    if(rio_writen(clientfd, req_buf, strlen(req_buf)) < 0)
    {
        printf("bufrequest error:%s\n", req_buf);
        fprintf(stderr, "rio_writen request error\n");
        close(clientfd);    
        return -1;
    }
    
    return 0;
}

int forward_response(rio_t *rio_server, char *resp_buf, int connfd)
{
    char response[MAX_OBJECT_SIZE];
    char resp_line[MAXLINE];
    int length = 0;
    int content_len = 0;
    #if 0
    /* Send response headers to client */
    while((length = rio_readnb(rio_server, response, strlen(resp_buf))) > 0)
    {

        DEBUG_PRINT(response);
         //Todo: cache response here
         
        /* send response to client*/
        if(rio_writen(connfd, response, length) < 0)
        {
            printf("rio_writen() error: %s\n", strerror(errno));
            fprintf(stderr, "rio_writen response error\n");
            return -1;
        }
        
    }
    #endif

    /*send reponse line and headers to client*/
    while((length = rio_readlineb(rio_server, resp_line, MAXLINE)) > 0)
    {
        strcat(response, resp_line);
        if(strcmp(resp_line, "\r\n") == 0)
            break;
        /* get size of response body from response header: Content-Length */
        if (strstr(resp_line, "Content-Length: ")) {
            content_len = parse_num(resp_line);
            if(content_len < 0)
            {
                fprintf(stderr, "Error get Content-Length: %s", resp_line);
                return -1;
            }
            printf("**** content_len: %d\n", content_len);
        }
    }
    
    /* Send response body to client */

    
    return 0;
}

int run_proxy(int connfd)
{
    char hostname[MAXLINE];
    char req_buf[MAX_OBJECT_SIZE];
    char resp_buf[MAX_OBJECT_SIZE];
    int port = 0;
    int clientfd;
    rio_t rio;

    /* read request */
    if(read_request(connfd, req_buf, hostname, &port) < 0)
        return -1;
    /* open connection to server */
    if((clientfd = Open_clientfd(hostname, port)) < 0)
    {
        printf("Open_clientfd error\n");
        fprintf(stderr, "Error: connection refused: %s !\n", hostname);
        return -1;
    }

    if(forward_request(&rio, req_buf, clientfd) < 0)
    {
        printf("forward_request error\n");
        fprintf(stderr, "Error: Send request to server failed !\n");
        return -1;
    }
    
    if(forward_response(&rio, resp_buf, connfd) < 0)
    {
        printf("forward_response\n");
        return -1;
    }
        
    
    Close(clientfd);
    Close(connfd);
    return 0;
}

void check_request(PRequest pRequest)
{

        
    
    return;
}

int main(int argc, char **argv)
{
    //printf("%s%s%s", user_agent, accept, accept_encoding);
    int listenfd, connfd, port;
    struct sockaddr_in clientaddr;
    socklen_t clientlen;
    /* Check command line args */
    if (argc != 2) {
        fprintf(stderr, "proxy usage: %s <port>\n", argv[0]);
        exit(1);
    }
    port = atoi(argv[1]);

    /* ignore SIGPIPE */
    Signal(SIGPIPE, SIG_IGN);


    /* open proxy listen port */
    listenfd = Open_listenfd(port);
    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        //doit(connfd);
        run_proxy(connfd);

    }
    return 0;
}
