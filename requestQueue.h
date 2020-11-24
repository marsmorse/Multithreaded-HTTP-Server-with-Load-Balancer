

#include <pthread.h>

typedef struct RequestObj* Request;
typedef struct RequestQueueObj* RequestQueue;

    Request getRequest(RequestQueue this, pthread_mutex_t* prgMutex);
    void pushRequest(RequestQueue this, int listen_socketd, pthread_mutex_t* prgMutex, pthread_cond_t* prgCondVar);
    RequestQueue newRequestQueue();
    int getRequestNumber(RequestQueue this);
    int getListenSockd(Request this);
    int getServerSockd(Request this);
    void clearRequests(RequestQueue this);
//expects all requests being pushed to it to be no bigger thatn 4096
