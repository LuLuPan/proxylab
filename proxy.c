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
#define MAX_THREADS 32

#define MIN(x, y) ((x) > (y) ? (y) : (x))

#define DEBUG 0

#if DEBUG
#define DEBUG_PRINT(msg) printf("%s", msg)
#else
#define DEBUG_PRINT(msg)
#endif


static const char *user_agent = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *accept_msg = "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
static const char *accept_encoding = "Accept-Encoding: gzip, deflate\r\n";
static const char *connection = "Connection: close\r\n";
static const char *proxy_connection = "Proxy-Connection: close\r\n";

pthread_mutex_t  restrict_mutex;
sem_t mutex;
sem_t mutex_q;

/*dynamic fd task queue*/
int fd_q[8];


int insert_fd_q(int fd)
{
    int i = 0;
    P(&mutex_q);
    for(i = 0; i < 8; i++) {
        if(fd_q[i] == -1) {
            fd_q[i] = fd;
            V(&mutex_q);
            return i;
        }
    }
    V(&mutex_q);
    return -1;
}

int pop_fd_q()
{
    int i = 0;
    int fd = 0;
    P(&mutex_q);
    for(i = 0; i < 8; i++) {
        if(fd_q[i] != -1) {
            fd = fd_q[i];
            fd_q[i] = -1;
            V(&mutex_q);
            return fd;
        }
    }
    V(&mutex_q);
    return -1;
}


/*simple tester to verify connection*/
char test[128] = " \
    <html> \
    <head><title>test</title></head> \
    <body> \
    Proxy Lab Test:)  \
    </body>   \
    </html>   \
    ";

/*unitility functions declaration*/
int read_request(int connfd, char *req_buf, char *hostname, int *port);
int forward_request(rio_t *rio, char *req_buf, int clientfd);
int forward_response(rio_t *rio_server, char *resp_buf, int connfd);
void *proxy_thread(void *argp);
int run_proxy(int connfd);
int is_valid_fd(int fd);
pid_t gettid();
int parse_num(char *str);
char* find_string(char *buf, char *target);
int parse_reqhdr(char *buf, char *hostname, int *port);



int main(int argc, char **argv)
{
    int listenfd, connfd, port;
    struct sockaddr_in clientaddr;
    socklen_t clientlen;
    pthread_t tid;
    int err = 0;

    memset(fd_q, -1, sizeof(int)*8);

    /* init mutex, default attri */
    err = pthread_mutex_init(&restrict_mutex, NULL);   
    if (err != 0)
        fprintf(stderr, "pthread_mutex_init failed: %s\n", strerror(err));
    
    sem_init(&mutex, 0, 1);
    sem_init(&mutex_q, 0 , 1);


    
    /* Check command line args */
    if (argc != 2) {
        fprintf(stderr, "proxy usage: %s <port>\n", argv[0]);
        exit(1);
    }
    port = atoi(argv[1]);

    /* ignore SIGPIPE to ignore broken error*/
    Signal(SIGPIPE, SIG_IGN);

    //for (i = 0; i < MAX_THREADS; i++)
        //Pthread_create(&tid, NULL, proxy_thread, NULL);

    /* open proxy listen port */
    listenfd = Open_listenfd(port);
    
    
    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        insert_fd_q(connfd);
        /*thread timing issue could happen between accept and execuing thread
                 Normal: A:fd1->t1(fd1)->A:fd2->t2(fd2)
                 Abnormal: A:fd1->A:fd2->t1(fd2)->t2(fd2)
                 Use fd task queue to avoid this by insert-consume
                 */           
        Pthread_create(&tid, NULL, proxy_thread, NULL);
    }

    Close(listenfd);

    /*destroy mutex*/
    sem_destroy(&mutex);
    sem_destroy(&mutex_q);
    pthread_mutex_destroy(&restrict_mutex);
    if (err != 0)
        fprintf(stderr, "pthread_mutex_destroy failed: %s\n", strerror(err));
    
    return 0;
}




/*simple verification of current file descriptor*/
int is_valid_fd(int fd)
{
    return fcntl(fd, F_GETFD) != -1 || errno != EBADF;
}

/*get current thread id: system call*/
pid_t gettid()
{
    return syscall(SYS_gettid);
}


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

/*find key word in string*/
char* find_string(char *buf, char *target)
{
    if(buf == NULL || target == NULL)
        return 0;
    return strstr(buf, target);
}


/* request line parser*/
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
        *port = 80;//default port
     }
        
     return 0;
}

/*parse request line/headers, set default HEADER*/
int read_request(int connfd, char *req_buf, char *hostname, int *port)
{
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char req_tmp[MAXLINE];
    rio_t rio;
    int len = -1;
    ssize_t n = 0;
    
    /* Read request line */
    Rio_readinitb(&rio, connfd);
    
    if((n = rio_readlineb(&rio, buf, MAXLINE)) <= 0)
    {
        fprintf(stderr, "rio_readlineb 1 read request error: %s\n", strerror(errno));
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
        fprintf(stderr, "rio_readlineb 2 read request error: %s\n", strerror(errno));
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
        
        if((len = rio_readlineb(&rio, buf, MAXLINE)) < 0)
        {
            fprintf(stderr, "rio_readlineb 3 read request error: %s\n", strerror(errno));
            return -1;
        }
        if(len == 0)
            break;
        //DEBUG_PRINT(buf);
        
    }

    //Todo: if without above request details, always send them here

    /* request empty line separater */
    strcat(req_buf, "\r\n");

    //DEBUG_PRINT(req_buf);
    return 0;
}

/* forward request to server after parse/modify*/
int forward_request(rio_t *rio, char *req_buf, int clientfd)
{
    Rio_readinitb(rio, clientfd);
#if DEBUG    
    printf("forward_request: %d\n", clientfd);
    printf("===========================\n");
    printf("%s", req_buf);
    printf("===========================\ns");
#endif
    /* send req msg to server*/
    if(rio_writen(clientfd, req_buf, strlen(req_buf)) < 0)
    {
        printf("bufrequest error:%s\n", req_buf);
        fprintf(stderr, "rio_writen request error: %s\n", strerror(errno));
        close(clientfd);    
        return -1;
    }
    
    return 0;
}


/*forward response (lines/headers/body) to client*/
int forward_response(rio_t *rio_server, char *resp_buf, int connfd)
{
    char response[MAX_OBJECT_SIZE];
    char resp_body[MAX_OBJECT_SIZE];
    char resp_line[MAXLINE];
    int length = 0;
    int body_size = 0;
    int content_len = -1;
    int cont_size = 0;

    /*send reponse line and headers to client*/
    while((length = rio_readlineb(rio_server, resp_line, MAXLINE)) > 0)
    {
        strcat(response, resp_line);
        /* send headers to clinet*/
        if (rio_writen(connfd, resp_line, length) < 0) {
            fprintf(stderr, "Error: rio_writen() in forward_response header:  %s\n", strerror(errno));  
            DEBUG_PRINT("Error: rio_writen() in forward_response header\n");
            /*write EPIPE error*/
            //if(errno == EPIPE)
            //    continue;
            return -1;
        }
        
        /*empty line between headers and body*/
        if(strcmp(resp_line, "\r\n") == 0)
            break;
        
        /* get size of response body from response header: Content-Length */
        if (strstr(resp_line, "Content-Length: ")) {
            content_len = parse_num(resp_line);
            if(content_len < 0)
            {
                fprintf(stderr, "Error get Content-Length: %s", resp_line);
                DEBUG_PRINT("Error get Content-Length\n");
                Close(connfd);
                printf("@@@@ close fd: %d\n", connfd);
                return -1;
            }
        }

        memset(resp_line, 0, sizeof(resp_line));
    }



    /*w/o content length in response headers*/
    if(content_len == -1)
        content_len = MAX_OBJECT_SIZE;
    
    cont_size = MIN(content_len, MAX_OBJECT_SIZE);
    
    /* Send response body to client */
#if DEBUG
    /* send fake response body */
    Rio_writen(connfd, test, strlen(test));
#else

    while((length = rio_readnb(rio_server, resp_body, cont_size)) > 0)
    {
        if (rio_writen(connfd, resp_body, length) < 0) {
            fprintf(stderr, "rio_writen in forward_response body error: %s!", strerror(errno));
            DEBUG_PRINT("rio_writen in forward_response body error!");
            return -1;
            /*write EPIPE error*/
            //if(errno == EPIPE)
            //    continue;
        }
        body_size += length;
    }
#endif

#if 0
    srcfd = Open(filename, O_RDONLY, 0);
    srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
    Close(srcfd);
    Rio_writen(fd, srcp, filesize);
    Munmap(srcp, filesize);
#endif
    
    return 0;
}

/* modify open_clientfd with mutex to avoid concurrent racing */
int open_clientfd_mutex(char *hostname, int port)
{
	int clientfd;
	struct hostent *hp;
	struct sockaddr_in serveraddr;

	if ((clientfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		return -1; /* Check errno for cause of error */

	/* Fill in the server¡¯s IP address and port */
	P(&mutex);	
	if ((hp = gethostbyname(hostname)) == NULL)
		return -2; /* Check h_errno for cause of error */
	bzero((char *) &serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	bcopy((char *)hp->h_addr_list[0], (char *)&serveraddr.sin_addr.s_addr, hp->h_length);
	serveraddr.sin_port = htons(port);
	V(&mutex);
	/* Establish a connection with the server */
	if (connect(clientfd, (SA *) &serveraddr, sizeof(serveraddr)) < 0)
	return -1;
	return clientfd;
}


/*proxy executation routine*/
int run_proxy(int connfd)
{
    char hostname[MAXLINE];
    char req_buf[MAX_OBJECT_SIZE];
    char resp_buf[MAX_OBJECT_SIZE];
    int port = 80;
    int clientfd;
    rio_t rio;

    memset(hostname, 0, MAXLINE);
    memset(req_buf, 0, MAX_OBJECT_SIZE);
    memset(resp_buf, 0 , MAX_OBJECT_SIZE);

    /* read request */
    if(read_request(connfd, req_buf, hostname, &port) < 0) {
        printf("close fd: %d, tid: %d\n", connfd, gettid());
        Close(connfd);
        return -1;
    }

    /* open connection to server */
    if((clientfd = open_clientfd_mutex(hostname, port)) < 0)
    {
        //printf("connfd: %d, clientfd: %d, host: %s, tid: %d\n", connfd, clientfd, hostname, gettid());
        printf("Open_clientfd error\n");
        fprintf(stderr, "Error: connection refused: %s !\n", hostname);
        Close(connfd);
        return -1;
    }

    if(forward_request(&rio, req_buf, clientfd) < 0)
    {
        printf("forward_request error\n");
        fprintf(stderr, "Error: Send request to server failed !\n");
        Close(clientfd);
        Close(connfd);
        return -1;
    }
    
    if(forward_response(&rio, resp_buf, connfd) < 0)
    {
        printf("forward_response\n");
        fprintf(stderr, "Error: Send response to client failed !\n");
        Close(clientfd);
        Close(connfd);
        return -1;
    }
        
    
    Close(clientfd);
    Close(connfd);
    return 0;
}

/* thread handler: do proxy*/
void *proxy_thread(void *argp)
{
    int connfd; 
    pthread_detach(pthread_self());
    connfd = pop_fd_q();
    //printf("### proxy_thread: %d, tid: %d\n", connfd, gettid());
    run_proxy(connfd);

    return NULL;
}

