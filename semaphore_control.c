#include <stdio.h>
#include <sys/sem.h>
#include <stdlib.h>

struct sembuf lock = {0, -1, SEM_UNDO};
struct sembuf unlock = {0, 1, SEM_UNDO};

// Блокировка семафора
int lockSemaphore(int semId){
    //Захват семафора
    if(semop(semId, &lock, sizeof(lock) / sizeof(struct sembuf)) < 0){
        perror("Error semop");
        exit(EXIT_FAILURE);
    }
    return 0;
}

// Разблокировка семафора
int unlockSemaphore(int semId){
    //Разблокировка семафора
    if(semop(semId, &unlock, sizeof(unlock) / sizeof(struct sembuf)) < 0){
        perror("Error semop");
        exit(EXIT_FAILURE);
    }
    return 0;
}
