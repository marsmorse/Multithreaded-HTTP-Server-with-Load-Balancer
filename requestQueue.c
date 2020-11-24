#include  "requestQueue.h"
#include <pthread.h>
#include <stdlib.h>


typedef struct RequestObj{
    int listen_fd;
    struct RequestObj* next;
} RequestObj;

typedef struct RequestQueueObj{
    Request head;
    Request tail;
    int numReqs;
} RequestQueueObj;


Request getRequest(RequestQueue this,pthread_mutex_t* prgMutex){
        __uint8_t retCode;
        Request aRequest;
        if(this->numReqs > 0){
            aRequest = this->head;
            this->head = aRequest->next;
            if(this->head ==NULL){
                this->tail=NULL;
            }
            this->numReqs--;
        }else{
            aRequest = NULL;
        }
        retCode = pthread_mutex_unlock(prgMutex);
        if(retCode != 0){
                
        }
        return aRequest;
        }
    void pushRequest(RequestQueue this,int listen_socketd, pthread_mutex_t* prgMutex, pthread_cond_t* prgCondVar){
            __uint8_t retCode;
            Request aRequest = (Request)malloc(sizeof(RequestObj));
            if(!aRequest){
                
            }
            aRequest->listen_fd = listen_socketd;
            aRequest->next = NULL;
            
            retCode = pthread_mutex_lock(prgMutex);
            if(this->numReqs == 0){
                this->head = aRequest;
                this->tail = aRequest;
            }
            else{
                this->tail->next = aRequest;
                this->tail = aRequest;
            }
            this->numReqs++;
            retCode = pthread_mutex_unlock(prgMutex);
            retCode = pthread_cond_signal(prgCondVar);
            if(retCode != 0){
                
            }
            aRequest = NULL;
            
        }
int getRequestNumber(RequestQueue this){
    return this->numReqs;
}
int getListenSockd(Request this){
    return this->listen_fd;
}
//constructors


RequestQueue newRequestQueue(void){
    RequestQueue RQ = malloc(sizeof(RequestQueueObj));
    RQ->head = NULL;
    RQ->tail = NULL;
    RQ->numReqs = 0;
    return RQ;
}
void clearRequests(RequestQueue this){
    Request temp = this->head;
    while(temp->next != NULL){
        temp = temp->next;
        this->head->next = NULL;
        free(this->head);
        this->head = temp;
    }
    temp = NULL;
    this->head = NULL;
    this->tail = NULL;
}