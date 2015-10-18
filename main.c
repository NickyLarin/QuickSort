#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/sem.h>
#include <errno.h>
#include <string.h>

struct SharedArgs
{
    int *shMem;
    int *semId;
    int *MAX_PROC;
    int *MAX_R_CALLS;
    int rCallsCount;
};
typedef struct SharedArgs SharedArgs;

struct sembuf lock = {0, -1, 0};
struct sembuf unlock = {0, 1, 0};

int createChildProcess(SharedArgs *sharedArgs);
void stopChildProcess(SharedArgs *sharedArgs);
void quickSort(int *array, int first, int last, SharedArgs *sharedArgs);
int lockSemaphore(int semId);
int unlockSemaphore(int semId);

int main(int argc, char *argv[])
{
    //Проверка аргументов
    if(argc < 3){
        errno = EINVAL;  //Invalid argument
        perror("Error arguments");
        exit(EXIT_FAILURE);
    }

    //Максимальное количество процессов - 1й аргумент
    int MAX_PROC = atoi(argv[1]);
    //Максимальное количество рекурсивных вызовов - 2й аргумент
    int MAX_R_CALLS = atoi(argv[2]);


    //Массив для сортировки
    int array[] = {11, 42, 2, 145, 33, 60, 25, 28, 8, 7, 49, 12, 24, -1, 15, 91, 20, 29, 2, 20};


    //Выделение shared memory
    //Счётчик процессов(int), IDs дочерних процессов(int), Размер массива(int), Массив(int)
    int sizeOfShMem = sizeof(int) + (sizeof(pid_t)*MAX_PROC) + sizeof(array);

    int arrayBeginIndex = (0 + sizeOfShMem - sizeof(array))/sizeof(array[0]);
    int arrayLength = sizeof(array)/sizeof(array[0]);

    int shMemId = shmget(IPC_PRIVATE, sizeOfShMem, SHM_W | SHM_R | IPC_CREAT);
    if(shMemId < 0){
        perror("Error shared memory");
        exit(EXIT_FAILURE);
    }

    //Привязка shmem к процессу
    int *shMem = (int *)shmat(shMemId, NULL, 0);

    //Счётчик процессов - первая "ячейка" shMem
    shMem[0] = MAX_PROC;

/*
    //Инициализация списка массива процессов
    for(int i = 1; i < (1 + MAX_PROC); i++)
    {
        shMem[i] = 0;
    }
*/

    //Копирование массива для сортировки в shmem
    memcpy(&shMem[arrayBeginIndex], array, sizeof(array));

    //Создание семафора
    int semId = semget(IPC_PRIVATE, 1, IPC_CREAT | IPC_EXCL | 0644);
    if(semId < 0){
        perror("Error semaphore get");
        exit(EXIT_FAILURE);
    }

    //Инициализация семафора нулём
    if(semctl(semId, 0, SETVAL, 1) < 0){
        perror("Error semaphore setval");
        exit(EXIT_FAILURE);
    }


    //Первоначальный массив в shmem
    for(int i = arrayBeginIndex; i < (arrayBeginIndex + arrayLength); i++){
        printf("%d ", shMem[i]);
    }
    printf("\n");

    SharedArgs sharedArgs;
    sharedArgs.shMem = shMem;
    sharedArgs.semId = &semId;
    sharedArgs.MAX_PROC = &MAX_PROC;
    sharedArgs.MAX_R_CALLS = &MAX_R_CALLS;
    sharedArgs.rCallsCount = MAX_R_CALLS;
/////////////////////////////////////////////////////////////////////////////////

    printf("Главный процесс: %d\n", getpid());

    sharedArgs.rCallsCount -= 1;

    //Первый дочерний процесс, сортировка запускается в нём
    int childProcess = createChildProcess(&sharedArgs);
    if(childProcess == 0){
        quickSort(&shMem[arrayBeginIndex], 0, arrayLength, &sharedArgs);
    }
/////////////////////////////////////////////////////////////////////////////////

    //Продолжение работы главного процесса
    else{
        printf("Main rCallsCount: %d\n", sharedArgs.rCallsCount);
        int status;
        printf("shMem[0] %d\nКол-во процессов %d\n", shMem[0], (MAX_PROC - shMem[0]));
        lockSemaphore(semId);
        while(shMem[0] < MAX_PROC)
        {
            unlockSemaphore(semId);
            wait(&status);
        }

        printf("Дождались завершения дочерних процессов\n");

        //Массив после сортировки
        printf("Массив после сортировки: \n");
        for(int i = arrayBeginIndex; i < (arrayBeginIndex + arrayLength); i++){
            printf("%d ", shMem[i]);
        }
        printf("\n");

        //Уничтожение семафора
        if(semctl(semId, 0, IPC_RMID) < 0){
            perror("Error semaphore remove");
        }

        //Отсоединение памяти и удаление сегмента
        shmdt(shMem);
        shmctl(shMemId, IPC_RMID, NULL);
    }

    return 0;
}

//Создание нового процесса
int createChildProcess(SharedArgs *sharedArgs)
{
    //Захват семафора
    lockSemaphore(*sharedArgs->semId);

    //Уменьшаем счётчик процессов на один
    sharedArgs->shMem[0] -= 1;

    pid_t newProcess = fork();

    //Дочерний процесс
    if(newProcess == 0){
        //Записываем Id процесса в общую память
        for(int i = 1; i < (1 + *sharedArgs->MAX_PROC); i++){
            if(sharedArgs->shMem[i] == 0){
                sharedArgs->shMem[i] = newProcess;
                break;
            }
        }
        sharedArgs->rCallsCount = *sharedArgs->MAX_R_CALLS;
        return(0);
    }

    //Родительский процесс
    else if(newProcess > 0){
        //Разблокировка семафора
        unlockSemaphore(*sharedArgs->semId);
        return(1);
    }

    else{
        perror("Error fork");
        exit(EXIT_FAILURE);
    }
}

//Завершение работы дочернего процесса
void stopChildProcess(SharedArgs *sharedArgs)
{
    //Захват семафора
    lockSemaphore(*sharedArgs->semId);

    sharedArgs->shMem[0] += 1;
    int currentPId = getpid();
    for(int i = 1; i < (1 + *sharedArgs->MAX_PROC); i++){
        if(sharedArgs->shMem[i] == currentPId){
            sharedArgs->shMem[i] = 0;
            break;
        }
    }

    //Разблокировка семафора
    unlockSemaphore(*sharedArgs->semId);
}

void swap(int *a, int *b)
{
    int temp = *a;
    *a = *b;
    *b = temp;
}

//Быстрая сортировка
void quickSort(int *array, int first, int last, SharedArgs *sharedArgs)
{
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


    if(l < last){
        quickSort(array, l, last, sharedArgs);
    }
    if(first < r){
        quickSort(array, first, r, sharedArgs);
    }
    sharedArgs->rCallsCount += 1;
    if(sharedArgs->rCallsCount == *sharedArgs->MAX_R_CALLS)
    {
        stopChildProcess(sharedArgs);
        printf("Дочерний процесс завершен\n");
        exit(EXIT_SUCCESS);
    }
}

int lockSemaphore(int semId)
{
    //Захват семафора
    if(semop(semId, &lock, sizeof(lock) / sizeof(struct sembuf)) < 0)
    {
        perror("Error semop");
        exit(EXIT_FAILURE);
    }
    return 0;
}

int unlockSemaphore(int semId)
{
    //Разблокировка семафора
    if(semop(semId, &unlock, sizeof(unlock) / sizeof(struct sembuf)) < 0)
    {
        perror("Error semop");
        exit(EXIT_FAILURE);
    }
    return 0;
}
