#include  "serverMinHeap.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <stdatomic.h>
#include <stdint.h>
#include <string.h>

#define SORTING 1
#define NOT_SORTING  0
typedef struct ServerObj{
    int server_fd;
    atomic_uint_fast32_t loads;
    atomic_uint_fast32_t fails;
    uint_fast16_t port_number;
    atomic_int_fast8_t fail_flag;
} ServerObj;

typedef struct ServersHeapObj{
    Server* server_list;
    int server_count;
    int size;
    pthread_mutex_t serverMutex;
    pthread_cond_t server_cond_var;
    atomic_int_fast8_t is_sorting;
    atomic_int_fast8_t processing_threads;
    
} ServersHeapObj;
/*
    HEAP GETTER FUNCTIONS
*/
    int getServersCount(ServersHeap this){
        return this->server_count; 
    }
    int getWorkingServersCount(ServersHeap this){
        return this->size;
    }

Server getServer(ServersHeap this,pthread_mutex_t* prgMutex){
        __uint8_t retCode;

        if(this->server_count > 0){

        }else{

        }
        retCode = pthread_mutex_unlock(prgMutex);
        if(retCode != 0){
                exit(1);
        }
        return this->server_list[0];
        }
void UpdateServerStatuses(ServersHeap this,int client_socketd, pthread_mutex_t* prgMutex, pthread_cond_t* prgCondVar){
            __uint8_t retCode;
            Server aServer = (Server)malloc(sizeof(ServerObj));
            if(!aServer){
                
            }
            aServer->server_fd = client_socketd;
            
            retCode = pthread_mutex_lock(prgMutex);
            if(this->server_count == 0){

            }
            else{

            }
            
            retCode = pthread_mutex_unlock(prgMutex);
            retCode = pthread_cond_signal(prgCondVar);
            if(retCode != 0){
                
            }
            aServer = NULL;
            
        }

/*
    HEAP OPERATION FUNCTIONS
*/
    void BuildHeap(ServersHeap this){
        for(int i = this->server_count/2;i > 0; i--){
            Heapify(this, i);
        }
        return;
    }
    void HeapSort(ServersHeap this, int index, uint_fast8_t singleHeapifyFlag){
       // printf("inside heapsort\n");
       printf("\033[0;31m");
        int retcode  = pthread_mutex_lock(&this->serverMutex);
        if(retcode > 0){
            fprintf(stderr, "error locking server mutex: %s\n", strerror(errno));
            exit(1);
        }
        if(singleHeapifyFlag == 1){
            Heapify(this, index);
        }else{
            BuildHeap(this);
        }
        markServerAsNotSorting(&this->is_sorting);
        pthread_cond_signal(&this->server_cond_var);
        pthread_mutex_unlock(&this->serverMutex);
        printf("\033[0m");
    }
    void Heapify(ServersHeap this, int index){
        //printf("calling heapify on port %ld at index %d\n", this->server_list[index]->port_number, index);
        int l = Left(index);
        int r = Right(index);
        int minimum = index;
        if(l <= this->server_count){
            //if left child is less than its parent (our current index)
            if(compareLT(this->server_list[index], this->server_list[l])){
                //printf("server at port %ld is less than left child server at port %ld\n", this->server_list[index]->port_number,this->server_list[l]->port_number);
                minimum = l;
            }
        }if(r <= this->server_count){
            //if the right child node is less than current min (left or original parent node) than its parent (our current index)
            if(compareLT(this->server_list[minimum], this->server_list[r])){
                //printf("server at port %ld is less than right child server at port %ld\n", this->server_list[index]->port_number,this->server_list[l]->port_number);       
                minimum = r;
            }
        }
        if(minimum != index){
            swapServers(this->server_list[index], this->server_list[minimum]);
            Heapify(this, minimum);
        }
    }
    //returns 1 if the child server is less than its parent
    //we are using it with copies of the serverList so we can guarantee exclusive access to the Server objects
    //so we aren't using atomic operations 
    int compareLT(Server parent_server, Server child_server){
        if(parent_server->fail_flag == 1 &&  child_server->fail_flag == 0){ 
            return 1;
        }else if(parent_server->fail_flag == 0 &&  child_server->fail_flag == 1){
            return 0;
        }else if(parent_server->fail_flag == 1 &&  child_server->fail_flag == 1){
            return 0;
        }
        else{
            if(child_server->loads == parent_server->loads ){
                if(parent_server->fails > child_server->fails){
                    return 1;
                }else{
                    return 0;
                }
            }else if(child_server->loads < parent_server->loads){
                return 1;
            }else{
                return 0;
            }
        }
    }
    void swapServers(Server s1, Server s2){
        //printf("Swapping Servers!!!\n");
        ServerObj temp;
        temp.server_fd = s1->server_fd;
        temp.loads  = s1->loads;
        temp.fails  = s1->fails;
        temp.port_number  = s1->port_number;
        temp.fail_flag  = s1->fail_flag;
        
        s1->server_fd = s2->server_fd;
        s1->loads  = s2->loads;
        s1->fails  = s2->fails;
        s1->port_number  = s2->port_number;
        s1->fail_flag  = s2->fail_flag;
    
        s2->server_fd = temp.server_fd;
        s2->loads  = temp.loads;
        s2->fails  = temp.fails;
        s2->port_number  = temp.port_number;
        s2->fail_flag  = temp.fail_flag;
    }
    int Parent(int i){
        return(i/2);
    }
    int Left(int i){
        return(2*i);
    }
    int Right(int i){
        return(2*i+1);
    }
/*
    SERVER OBJECT GETTER FUNCTIONS
*/
    int getServerPortNumber(ServersHeap this,int index){
        if(index < this->server_count+1 && index >0){
            return(this->server_list[index]->port_number);
        }else{
            printf("[getServerPortNumber] ERROR tried to get data from a server outside the bounds of the servers array");
            return 0;
        }
        return 0;
    }
    int getServerfd(Server this){
        return this->server_fd;
    }
/*
    FLAG SETTER AND GETTERS
*/
    void markServerAsDown(ServersHeap this, int index){
        uint_fast8_t val = 1;
        if(index < this->server_count+1 && index >0){
            __atomic_store(&(this->server_list[index]->fail_flag), &val, __ATOMIC_RELAXED);
        }else{
            fprintf(stderr, "[MarkServerAsDown] ERROR tried to get data from a server outside the bounds of the servers array at index %d", index);
            exit(1);
        }
    }
    void markServerAsUp(ServersHeap this, int index){
        uint_fast8_t val = 0;
        if(index < this->server_count+1 && index >0){
            __atomic_store(&(this->server_list[index]->fail_flag), &val, __ATOMIC_RELAXED);
        }else{
            fprintf(stderr, "[MarkServerAsDown] ERROR tried to get data from a server outside the bounds of the servers array at index %d", index);
            exit(1);
        }
    }
    //sets sorting flag to 1, if already at 1 pri   
    void markServerAsSorting(atomic_int_fast8_t* flag){
        uint_fast8_t val = 1;
        __atomic_store((flag), &val, __ATOMIC_RELAXED);
    }
    //same as setting the sorting flag but sets it to 0 and returns an error if already at 0
    void markServerAsNotSorting(atomic_int_fast8_t* flag){
        uint_fast8_t val = 0;
        __atomic_store((flag), &val, __ATOMIC_RELAXED);
    }
/*
    HANDY FUNCTIONS
*/
    int getNextServer(ServersHeap this){
        uint_fast8_t sorting_flag;
        __atomic_load(&(this->is_sorting), &sorting_flag,__ATOMIC_RELAXED);
        pthread_mutex_lock(&this->serverMutex);
        uint_fast16_t pn;
        while(1){
            if(sorting_flag == NOT_SORTING){
                pn = this->server_list[1]->port_number;
                __atomic_fetch_add(&(this->server_list[1]->loads), 1, __ATOMIC_RELAXED);
                //set sorting flag to SORTING
                markServerAsSorting(&(this->is_sorting));
                //printf("[getNextServer] sorting flag is %d\n", this->is_sorting);
                pthread_mutex_unlock(&this->serverMutex);
                HeapSort(this, 1, 1);
                markServerAsNotSorting(&this->is_sorting);
                //check if all servers are down
                if(this->server_list[1]->fail_flag){
                    return -1;
                }
                return pn;
            }else{
                pthread_cond_wait(&this->server_cond_var, &this->serverMutex); 
            }
        } 
    }
    void addServer(ServersHeap this, int s_fd, uint_fast32_t loads, uint_fast32_t fails, const int port_number, uint_fast8_t fail_flag){
        Server temp = malloc(sizeof(ServerObj));
        temp->server_fd = s_fd;
        temp->loads = loads;
        temp->fail_flag = fail_flag;
        temp->port_number = port_number;
        temp->fails = fails;
    
        if(this->size < this->server_count){
            this->server_list[this->size+1] = temp;
            this->size ++;
            temp = NULL;
        }else{
            exit(1);
        }
        
    }
    void printServers(ServersHeap this){
        //printf("[printServers] Servers Object has %d total servers, %d working servers\r\n\r\n", this->server_count, this->size);
        for(int i = 1;i < this->server_count+1; i++){
            printf("Server at port %ld:\n",this->server_list[i]->port_number);
            printf("  -%ld total requests\n",this->server_list[i]->loads);
            printf("  -%ld failures\n",this->server_list[i]->fails);
            printf("  -fail flag is %d\n",this->server_list[i]->fail_flag);
        }
    }
    void updateServer(ServersHeap this,uint_fast32_t loads,uint_fast32_t fails,uint_fast8_t fail_flag, int index){
        printf("[updateServer] index of server to be updated is %d and its fail flag is %d\n", index, fail_flag);
        if(index < this->server_count+1 && index >0){
            __atomic_store(&(this->server_list[index]->loads), &loads, __ATOMIC_RELAXED);
            __atomic_store(&(this->server_list[index]->fails), &fails, __ATOMIC_RELAXED);
            __atomic_store(&(this->server_list[index]->fail_flag), &fail_flag, __ATOMIC_RELAXED);
        }else{
            printf("[updateServer] ERROR tried to get data from a server outside the bounds of the servers array");
            exit(1);
        }  
    }
/*
    Heap maintenance functions
*/

//constructor and destructor
    void clearServers(ServersHeap this){
        for(int i = 1;i < this->server_count; i++){
           free(this->server_list[i]);
           this->server_list[i] = NULL;
        }
        free(this->server_list);
        this->server_list = NULL;
        pthread_mutex_destroy(&this->serverMutex);
    }
    ServersHeap newServersHeap(int N){
        ServersHeap SH = malloc(sizeof(ServersHeapObj)); 
        SH->server_list = malloc((N+1) * sizeof(ServerObj));
        SH->server_count = N;
        SH->size = 0;
        SH->is_sorting = 0;
        int retcode = pthread_mutex_init(&(SH->serverMutex),NULL);
        if(retcode > 0){
            fprintf(stderr,"ERROR initializing server mutex: %s\n", strerror(errno));
            exit(1);
        }
        return SH;
    }
