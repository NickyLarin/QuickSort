#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

#include "semaphore_control.h"

#ifndef SHARED_ARGS_H
    struct SharedArgs{
        int *shMem;
        int *semId;
        int *MAX_PROC;
        int *MAX_R_CALLS;
        int rCallsCount;
    };
    typedef struct SharedArgs SharedArgs;
    #define SHARED_ARGS_H
#endif

// Создание нового процесса
int createChildProcess(SharedArgs *sharedArgs){
    printf("Процесс: %d пытается создать новый дочерний\n", getpid());
    // Захват семафора
    lockSemaphore(*sharedArgs->semId);
    if(sharedArgs->shMem[0] > 0){
        // Уменьшаем счётчик процессов на один
        sharedArgs->shMem[0] -= 1;

        pid_t newProcess = fork();

        // Дочерний процесс
        if(newProcess == 0){
            sharedArgs->rCallsCount = *sharedArgs->MAX_R_CALLS;
            return 0;
        }

        // Родительский процесс
        else if(newProcess > 0){
            // Записываем Id процесса в общую память
            for(int i = 1; i < (1 + *sharedArgs->MAX_PROC); i++){
                if(sharedArgs->shMem[i] == 0){
                    sharedArgs->shMem[i] = newProcess;
                    printf("Процесс %d записан в ячейку %d\n", newProcess, i);
                    break;
                }
            }
            // Разблокировка семафора
            unlockSemaphore(*sharedArgs->semId);
            return newProcess;
        }
        // Возвращение -1 при ошибке вызова fork() !!!
        else{
            perror("Error fork");
            return -1;
        }
    }

    // Если уже достигнуто максимальное количество процессов
    else{
        // Разблокировка семафора
        unlockSemaphore(*sharedArgs->semId);
        return -1;
    }
}

// Завершение работы дочернего процесса
void stopChildProcess(SharedArgs *sharedArgs){
    // Захват семафора
    lockSemaphore(*sharedArgs->semId);

    sharedArgs->shMem[0] += 1;
    int currentPId = getpid();
    for(int i = 1; i < (1 + *sharedArgs->MAX_PROC); i++){
        if(sharedArgs->shMem[i] == currentPId){
            sharedArgs->shMem[i] = 0;
            break;
        }
    }

    // Разблокировка семафора
    unlockSemaphore(*sharedArgs->semId);
    printf("Дочерний процесс: %d завершен\n", currentPId);
}
