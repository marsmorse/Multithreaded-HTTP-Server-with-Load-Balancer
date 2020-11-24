#include <stdint.h>
#include <stdatomic.h>
#include <pthread.h>


typedef struct ServerObj* Server;
typedef struct ServersHeapObj* ServersHeap;

//getter functions
    int getServersCount(ServersHeap this);
    int getWorkingServersCount(ServersHeap this);
    int getNextServer(ServersHeap this);
    int getServerPortNumber(ServersHeap this,int index);
// heap operation functionsint Parent(int i)
    void BuildHeap(ServersHeap this);
    void HeapSort(ServersHeap this, int index, uint_fast8_t singleHeapifyFlag);
    void Heapify(ServersHeap this, int index);
    void swapServers(Server s1, Server s2);
    int compareLT(Server parent_server, Server child_server);
    int Parent(int i);
    int Left(int i);
    int Right(int i);
//flag setter and getter's
    void markServerAsSorting(atomic_int_fast8_t* flag);
    void markServerAsNotSorting(atomic_int_fast8_t* atomic_flag);
    void markServerAsDown(ServersHeap this, int index);
    void markServerAsUp(ServersHeap this, int index);
//handy functions
    void addServer(ServersHeap this, int s_fd, uint_fast32_t loads, uint_fast32_t fails, int port_number, uint_fast8_t fail_flag);
    void printServers(ServersHeap this);
    void updateServer(ServersHeap servers,uint_fast32_t loads,uint_fast32_t fails, uint_fast8_t fail_flag, int index);
    void UpdateServerStatuses(ServersHeap this, int client_socketd, pthread_mutex_t* prgMutex, pthread_cond_t* prgCondVar);

//constructor and destructor 
    ServersHeap newServersHeap(int N);
    void clearServers(ServersHeap this);

