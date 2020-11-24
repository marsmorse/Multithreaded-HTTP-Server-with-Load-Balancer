
#include<err.h>
#include<arpa/inet.h>
#include<netdb.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<sys/time.h>
#include<unistd.h>
#include "requestQueue.h"
#include "serverMinHeap.h"
#include<getopt.h>
#include <stdatomic.h>
#include <errno.h>
#include <stdint.h>


typedef struct Health {
    uint_fast32_t loads;
    uint_fast32_t fails;
    uint_fast8_t fail_flag;
} Health;

typedef struct sharedData{
    ServersHeap sh;
    RequestQueue rq;
    uint_fast8_t* rc;
} SharedData;
//incoming request lock and conditional variable
pthread_mutex_t req_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t got_request = PTHREAD_COND_INITIALIZER;

pthread_mutex_t health_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t health_check = PTHREAD_COND_INITIALIZER;
/*
 * client_connect takes a port number and establishes a connection as a client.
 * establish multiple client connections as the load balancer
 * connectport: port number of server to connect to
 * returns: valid socket if successful, -1 otherwise
 */
int client_connect(uint16_t connectport) {
    int connfd;
    struct sockaddr_in servaddr;

    connfd=socket(AF_INET,SOCK_STREAM,0);
    if (connfd < 0)
        return -1;
    memset(&servaddr, 0, sizeof servaddr);

    servaddr.sin_family=AF_INET;
    servaddr.sin_port=htons(connectport);

    /* For this assignment the IP address can be fixed */
    inet_pton(AF_INET,"127.0.0.1",&(servaddr.sin_addr));

    if(connect(connfd,(struct sockaddr *)&servaddr,sizeof(servaddr)) < 0)
        return -1;
    return connfd;
}
/*
 * server_listen takes a port number and creates a socket to listen on 
 * that port.
 * port: the port number to receive connections
 * returns: valid socket if successful, -1 otherwise
 */
int server_listen(int port) {
    int listenfd;
    int enable = 1;
    struct sockaddr_in servaddr;

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0)
        return -1;
    memset(&servaddr, 0, sizeof servaddr);
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htons(INADDR_ANY);
    servaddr.sin_port = htons(port);

    if(setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0)
        return -1;
    if (bind(listenfd, (struct sockaddr*) &servaddr, sizeof servaddr) < 0)
        return -1;
    if (listen(listenfd, 500) < 0)
        return -1;
    return listenfd;
}
/*
 * bridge_connections send up to 100 bytes from fromfd to tofd
 * fromfd, tofd: valid sockets
 * returns: number of bytes sent, 0 if connection closed, -1 on error
 */
int bridge_connections(int fromfd, int tofd) {
    char recvline[100];
    int n = recv(fromfd, recvline, 100, 0);
    if (n < 0) {
        fprintf(stderr, "connection error receiving\n");
        return -1;
    } else if (n == 0) {
        fprintf(stderr, "receiving connection ended\n");
        return 0;
    }
    recvline[n] = '\0';
    //printf("%s", recvline);
    n = send(tofd, recvline, n, 0);
    if (n < 0) {
        fprintf(stderr, "connection error sending\n");
        return -1;
    } else if (n == 0) {
        fprintf(stderr, "sending connection ended\n");
        return 0;
    }
    //printf("sent %d bytes\n", n);
    return n;
}

/*
 * bridge_loop forwards all messages between both sockets until the connection
 * is interrupted. It also prints a message if both channels are idle.
 * sockfd1, sockfd2: valid sockets
 */
void bridge_loop(int sockfd1, int sockfd2) {
    fd_set set;
    struct timeval timeout;

    int fromfd, tofd;
    while(1) {
        // set for select usage must be initialized before each select call
        // set manages which file descriptors are being watched
        FD_ZERO (&set);
        FD_SET (sockfd1, &set);
        FD_SET (sockfd2, &set);

        // same for timeout
        // max time waiting, 5 seconds, 0 microseconds
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;

        // select return the number of file descriptors ready for reading in set
        switch (select(FD_SETSIZE, &set, NULL, NULL, &timeout)) {
            case -1:
                fprintf(stderr, "error during select:%s\n", strerror(errno));
                return;
            case 0:
                printf("both channels are idle, waiting again\n");
                continue;
            default:
                if (FD_ISSET(sockfd1, &set)) {
                    fromfd = sockfd1;
                    tofd = sockfd2;
                } else if (FD_ISSET(sockfd2, &set)) {
                    fromfd = sockfd2;
                    tofd = sockfd1;
                } else {
                    printf("this should be unreachable\n");
                    return;
                }
        }
        if (bridge_connections(fromfd, tofd) <= 0)
            return;
    }
}
//makes a healthcheck request to a client and returns results in a health object
//populates the Health struct with 
uint_fast8_t healthCheck(Health* response, int connect_fd){
    fd_set rfds;
    struct timeval tv;
    int retval;
    tv.tv_sec = 5;
    tv.tv_usec = 0;

    FD_ZERO(&rfds);
    FD_SET(connect_fd, &rfds);
    
    char healthcheck_msg[] = "GET /healthcheck HTTP/1.1\r\n\r\n";
    int bytes_sent = send(connect_fd, healthcheck_msg, strlen(healthcheck_msg), 0);
    if(bytes_sent < 0){
        fprintf(stderr,"Failed to send healthcheck to the server\n");
        response->fail_flag = 1;
        return 1;
    }
    int bytes_read=0;
    char buf[200];
    bytes_read = recv(connect_fd,buf, sizeof(buf), 0);
    buf[bytes_read] = '\0';
    //printf("sent %d bytes to server\n", bytes_sent);
    
    retval = select(FD_SETSIZE, &rfds, NULL, NULL, &tv);
    if (retval == -1){
        perror("select() returned -1");
        response->fail_flag = 1;
        return 1;
    }//parse recieved message and put it into the Health struct
    else if (retval){
        //printf("[healthcheck] Data is available now. parsing it\n");
        char* savePtr;
        char* token;
        bytes_read--;
        if(strlen(buf)<15){
            response->fail_flag = 1;
            return 1;
        }
        token = strtok_r(buf, "\r\n\r\n",&savePtr);
        //printf("healthcheck response is %s and length is %ld\n", buf, strlen(buf) );
        //printf("first token is %s\n", token);

        char* subtoken;
        char* subSavePtr;
        subtoken = strtok_r(token, " ",&subSavePtr);
        if(subtoken == NULL){
            response->fail_flag = 1;
            return 1;
        }
        //printf("first subtoken is %s\n", subtoken);
        subtoken = strtok_r(NULL, " ",&subSavePtr);
        if(subtoken == NULL){
            response->fail_flag = 1;
            return 1;
        }
        //printf("second subtoken is %s\n", subtoken);

        token = strtok_r(NULL, "\r\n\r\n",&savePtr);
        if(token == NULL){
            response->fail_flag = 1;
            return 1;
        }
        token = strtok_r(NULL, "\r\n",&savePtr);
        if(token == NULL){
            response->fail_flag = 1;
            return 1;
        }
        response->fails = atoi(token);
        token = strtok_r(NULL, "\r\n",&savePtr);
        if(token == NULL){
            response->fail_flag = 1;
            return 1;
        }
        //printf("total atresponsets token is %s\n", token);    
        response->loads = atoi(token);
        if(strcmp(subtoken,"500")==0){
            response->fail_flag = 1;
            return 1;
        }else{
            response->fail_flag = 0;
            return 0;
        }
            /* FD_ISSET(0, &rfds) will be true. */       
        }
    else{
        //printf("No data within five seconds.\n");
        response->fail_flag = 1;
        return 1;
    }
}
int updateServersStatus(ServersHeap servers){
    //printf("server port count %d", server_port_count);
    int connfd;
    Health temp;

    for(int i = 1; i < getServersCount(servers)+1;i++){
        //printf("{updatingServersStatus] starting loop at index %d\n",i);
        uint16_t connectport = getServerPortNumber(servers, i);
        //printf("{updatingServersStatus] connect port is %d\n",connectport);
        if ((connfd = client_connect(connectport)) < 0){
           printf("{updatingServersStatus] connection failed with error code of %s\n",strerror(errno));
            temp.fail_flag = 1;
        }
        else if(healthCheck(&temp, connfd) == 1){
            printf("{updatingServersStatus] Healthcheck reported server as down\n");
            temp.fail_flag = 1;
        }
        //pthread_mutex_lock(getServerLock(servers));
        //printf("{updatingServersStatus] calling update server\n");
        if(temp.fail_flag == 1){
            updateServer(servers, 0, 0, temp.fail_flag, i);
        }else{
            updateServer(servers, temp.loads, temp.fails, temp.fail_flag, i);           
        }
        close(connfd);
    }
    HeapSort(servers, 1, 0);
    printServers(servers);
    return 1;
}
void send500Response(int accept_fd){
    char server_err_resp[] = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\n\r\n";
    send(accept_fd, server_err_resp, strlen(server_err_resp), 0);
}
void handleRequest(int accept_fd, ServersHeap servers){
    //set text color to red
        int connect_port = getNextServer(servers);
        printf("\033[0;34m[#] chosen server %d\n\033[0m", connect_port);
        //all available servers are down
        if(connect_port == -1){
            //printf("All servers down\n");
            send500Response(accept_fd);
        }else{
            int connfd;
            if ((connfd = client_connect(connect_port)) < 0){
                //printf("Couldn't connect to port specified\n");
                send500Response(accept_fd); 
            }
            else{
                bridge_loop(accept_fd, connfd);
                close(connfd);
            }
        }   
}
void* handleRequests(void* data){
    //printf("\033[0;32m[#] A new thread has called handleRequests\n\033[0m");
    SharedData* sd = ((SharedData*)data);
    RequestQueue rq = (sd->rq);
    ServersHeap sh = (sd->sh);
    pthread_mutex_lock(&req_mutex);
    Request req;
    while(1){
        if(getRequestNumber(rq)>0){
            printf("\033[0;32m[#] worker is getting request socket_d from queue\n\033[0m");
            req = getRequest(rq, &req_mutex);
            if(req){
                handleRequest(getListenSockd(req),sh);
                free(req);
            }

        }else{
            pthread_cond_wait(&got_request, &req_mutex);
             //printf("\033[0;32m[#] worker signaled to Wake Up\n\033[0m");
        }
    }
}
void* healtcheckLoop(void* shared){
    SharedData* sd = (SharedData*)shared;
    ServersHeap servers = sd->sh;
    uint_fast8_t* req_count = sd->rc;
    struct timespec ts;
    struct timeval timeval;
    while(1){
       // printf("at the begginning of the loop\n");
        pthread_mutex_lock(&health_mutex);
        memset(&ts, 0, sizeof(ts));
        gettimeofday(&timeval,NULL);
        ts.tv_sec = timeval.tv_sec + 5;
       // printf("ts is %ld\n", ts.tv_sec);
        int timeout = pthread_cond_timedwait(&health_check, &health_mutex, &ts);

        if(timeout == 0){
            //printf("signalled!\n");
            *req_count = 0;
            pthread_mutex_unlock(&health_mutex);
        }else if(timeout == ETIMEDOUT){
            //printf("timed out after 5 seconds\n");
            *req_count = 0;
            pthread_mutex_unlock(&health_mutex);
        }
        
        updateServersStatus(servers);
        
    }
}
int main(int argc,char **argv) {
    uint_fast8_t Nflag = 0;
    if(Nflag){
        int j = 5;j++;
    }
    
    int threadNum = 4;
    uint_fast8_t RFlag = 0;
    int request_num = 5;
    char* portNumber = 0;
    uint_fast8_t portFlag = 0;
    int c;
    opterr = 0;
    char* serverPorts[argc];
    int server_port_count = 0;
    uint_fast8_t timeout = 5;
if(RFlag){
        int j = 5;j++;
    }
    while((c = getopt(argc, argv, "-N:R:"))!= -1){
        switch(c){
            case 'N':
                if(optarg == 0){
                        fprintf(stderr, "%c flag must have number after it\n", optopt);
                        exit(1);
                    }
                Nflag = 1;
                threadNum = atoi(optarg);
                break;
            case 'R':
                if(optarg == 0){
                        fprintf(stderr, "%c flag must a valid logfile after it\n", optopt);
                        exit(1);
                    }
                RFlag = 1;
                request_num = atoi(optarg);
                break;
            case 1:
                //non optionary character code when we use - as the first char in optstring
                if(portFlag == 0){
                    portNumber = optarg;
                    portFlag = 1;
                }else{
                    serverPorts[server_port_count] = optarg;
                    server_port_count ++;
                }
                break;
            case '?':
                //printf("%s\n",optopt);
                if(optopt == 'N'){
                    fprintf(stderr, "Option -%c requires an argument.\n", optopt);
                }else if(optopt == 'l'){
                    fprintf(stderr, "Option -%c requires an argument.\n", optopt);
                }else{
                    fprintf(stderr, "erroneous option character \\x%x'\n", optopt);
                }
                exit(1);
            default:
                abort();
        }
    }

    int listenfd, acceptfd;
    uint16_t listenport;
    if (argc < 3) {
        fprintf(stderr,"missing arguments: usage %s port_to_connect port_to_listen", argv[0]);
        exit(1);
    }
    
    // Remember to validate return values
    // You can fail tests for not validating
    listenport = atoi(portNumber);
    ServersHeap servers = newServersHeap(server_port_count);
    //printf("server port count %d\n", server_port_count);
    //creates server object at ports specified by input


    for(int i = 1; i < getServersCount(servers)+1;i++){
        addServer(servers, 0, 0, 0, atoi(serverPorts[i-1]), 0);
    }
    updateServersStatus(servers);
    //printServers(servers);
    if ((listenfd = server_listen(listenport)) < 0)
        err(1, "failed listening");
    //printf("hello\n");
    RequestQueue request_queue = newRequestQueue();   
    pthread_t p_threads[threadNum];
    uint_fast8_t request_count = 0;
    SharedData* threadData = malloc(sizeof(SharedData));
    threadData->rq = request_queue;
    threadData->sh = servers;
    threadData->rc = &request_count;
    for(uint_fast8_t i = 0; i < threadNum; i++){
        pthread_create(&p_threads[i], NULL, handleRequests,(void*)(threadData));
    }
    pthread_t health_thread;
    pthread_create(&health_thread, NULL, healtcheckLoop, (void*)threadData);
    while(1){
        if ((acceptfd = accept(listenfd, NULL, NULL)) < 0)
            err(1, "failed accepting");
        
        if( request_count >= request_num){
            pthread_cond_signal(&health_check);
        }
        pthread_mutex_lock(&health_mutex);
        request_count++;
        pthread_mutex_unlock(&health_mutex);

        pushRequest(request_queue, acceptfd, &req_mutex, &got_request);  
    }
    //connect to each server port, run healthchecks, and create a server heap
    
    //free memory for request queue
    clearRequests(request_queue);
    free(request_queue);
    request_queue = NULL;
    //free memory for server heap
    clearServers(servers);
    free(servers);
    servers = NULL;
    //free shared data
    threadData->rq = NULL;
    threadData->sh = NULL;
    free(threadData);
    threadData = NULL;

    pthread_mutex_destroy(&req_mutex);
    timeout --;
    //printf("we are dooooone\n");
    
}