#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "process_control.h"


#ifndef SHARED_ARGS_H
    struct SharedArgs
    {
        int *shMem;
        int *semId;
        int *MAX_PROC;
        int *MAX_R_CALLS;
        int rCallsCount;
    };
    typedef struct SharedArgs SharedArgs;
    #define SHARED_ARGS_H
#endif

void swap(int *a, int *b);

//Быстрая сортировка
void quickSort(int *array, int first, int last, SharedArgs *sharedArgs)
{
    printf("Процесс: %d начал сортировку first: %d last: %d rCallsCount: %d\n", getpid(), first, last, sharedArgs->rCallsCount);
    //Уменьшение счётчика рекурсивных вызовов
    sharedArgs->rCallsCount -= 1;

    int l = first;
    int r = last;
    int pivot = array[(first + last) / 2];

    do{
        while(array[l] < pivot){
            l++;
        }
        while(array[r] > pivot){
            r--;
        }

        if(l <= r){
            if(array[l] >= array[r]){
                swap(&array[l], &array[r]);
                l++;
                r--;
            }
        }
    }
    while(l <= r);

    if(sharedArgs->rCallsCount == 0){

    //Правая сторона при достижении максимального количества рекурсивных вызовов
        if(l < last){
            int newChildProcess = createChildProcess(sharedArgs);
            if(newChildProcess == 0){
                quickSort(array, l, last, sharedArgs);
            }
            else if(newChildProcess == -1){
                printf("Процессу: %d не удалось создать дочерний, попытка продолжить сортировку в том же\n", getpid());
                quickSort(array, l, last, sharedArgs);
            }
        }

    //Левая сторона при достижении максимального количества рекурсивных вызовов
        if(first < r){
            int newChildProcess = createChildProcess(sharedArgs);
            if(newChildProcess == 0){
                quickSort(array, first, r, sharedArgs);
            }
            else if(newChildProcess == -1){
                printf("Процессу: %d не удалось создать дочерний, попытка продолжить сортировку в том же\n", getpid());
                quickSort(array, first, r, sharedArgs);
            }
        }
    }
    else{
        //Правая сторона массива
        if(l < last){
            quickSort(array, l, last, sharedArgs);
        }

        //Левая сторона массива
        if(first < r){
            quickSort(array, first, r, sharedArgs);
        }
    }

    sharedArgs->rCallsCount += 1;
    //Завершение родительского процесса при достижении места, откуда он стартовал
    if(sharedArgs->rCallsCount == *sharedArgs->MAX_R_CALLS){
        stopChildProcess(sharedArgs);
        printf("Дочерний процесс завершен\n");
        exit(EXIT_SUCCESS);
    }
}

void swap(int *a, int *b)
{
    int temp = *a;
    *a = *b;
    *b = temp;
}
