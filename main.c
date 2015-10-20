#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/sem.h>
#include <errno.h>
#include <unistd.h>

#include "qsort.h"
#include "process_control.h"
#include "semaphore_control.h"
#include "readline.h"


int main(int argc, char *argv[]){
    // Проверка аргументов
    if(argc < 3){
        errno = EINVAL;  // Invalid argument
        perror("Error arguments");
        exit(EXIT_FAILURE);
    }

    // Максимальное количество процессов - 1й аргумент
    int MAX_PROC = atoi(argv[1]);
    // Максимальное количество рекурсивных вызовов - 2й аргумент
    int MAX_R_CALLS = atoi(argv[2]);

    // Чтение входных данных из файла
    int arraySize = 4096;
    int *array = (int *)malloc(sizeof(int)*arraySize);
    int arrayLength = 0;

    const char separator[] = " ";

    FILE *fptr;
    fptr = fopen("input.txt", "r");
    
    while(!feof(fptr)){
        char *line = readLine(fptr);
        char *ch = strtok(line, separator);
        while(ch != NULL){
            if(arrayLength == arraySize){
                arraySize *= 2;
                array = realloc(array, arraySize);
            }
            else{
                int temp = atoi(ch);
                array[arrayLength] = temp;
                arrayLength++;
            }
            ch = strtok(NULL, separator);
        }
        free(line);
    }
    fclose(fptr);

    // Выделение shared memory
    // Счётчик процессов(int), IDs дочерних процессов(int), Размер массива(int), Массив(int)
    int sizeOfShMem = sizeof(int) + (sizeof(pid_t)*MAX_PROC) + sizeof(int)*arrayLength;

    int arrayBeginIndex = (0 + sizeOfShMem - sizeof(int)*arrayLength) / sizeof(int);

    int shMemId = shmget(IPC_PRIVATE, sizeOfShMem, SHM_W | SHM_R | IPC_CREAT);
    if(shMemId < 0){
        perror("Error shared memory");
        exit(EXIT_FAILURE);
    }

    // Привязка shmem к процессу
    int *shMem = (int *)shmat(shMemId, NULL, 0);

    // Счётчик процессов - первая "ячейка" shMem
    shMem[0] = MAX_PROC;

    // Копирование массива для сортировки в shmem
    memcpy(&shMem[arrayBeginIndex], array, sizeof(int)*arrayLength);
    free(array);

    // Создание семафора
    int semId = semget(IPC_PRIVATE, 1, IPC_CREAT | IPC_EXCL | 0644);
    if(semId < 0){
        perror("Error semaphore get");
        exit(EXIT_FAILURE);
    }

    // Инициализация семафора единицей
    if(semctl(semId, 0, SETVAL, 1) < 0){
        perror("Error semaphore setval");
        exit(EXIT_FAILURE);
    }


    // Первоначальный массив в shmem
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

    // Первый дочерний процесс, сортировка запускается в нём
    int firstChildProcess = createChildProcess(&sharedArgs);

    if(firstChildProcess == 0){
        quickSort(&shMem[arrayBeginIndex], 0, arrayLength, &sharedArgs);
    }

    // Продолжение работы главного процесса
    else{

        // Ошибка: не удалось создать первый дочерний процесс
        if(firstChildProcess == -1){
            perror("Error creating first child process");
        }
        else{

            // Ожидание дочерних процессов
            printf("Начинаем ожидание процесса: %d\n", firstChildProcess);
            waitpid(firstChildProcess, 0, 0);
            printf("Ожидание процесса: %d завершено\n", firstChildProcess);
            
            do{
                printf("Начинаем ожидание остальных дочерних процессов\n");
                lockSemaphore(semId);
                if(shMem[0] == MAX_PROC){
                    unlockSemaphore(semId);
                    break;
                }
                else{
                    printf("Cписок процессов\n");
                    for(int i = 1; i < (1 + MAX_PROC); i++){
                        printf("%d\n", shMem[i]);
                        if(shMem[i] != 0){
                            unlockSemaphore(semId);
                            printf("Начинаем ожидание процесса: %d\n", shMem[i]);
                            waitpid(shMem[i], 0, 0);
                            printf("Ожидание процесса: %d завершено\n", shMem[i]);
                            break;
                        }
                    }
                }
                
            }while(1);

            printf("Дождались завершения дочерних процессов\n");

            // Массив после сортировки
            printf("Массив после сортировки: \n");
            for(int i = arrayBeginIndex; i < (arrayBeginIndex + arrayLength); i++){
                printf("%d ", shMem[i]);
            }
            printf("\n");

        }

        // Уничтожение семафора
        if(semctl(semId, 0, IPC_RMID) < 0){
            perror("Error semaphore remove");
        }

        // Отсоединение памяти и удаление сегмента
        shmdt(shMem);
        shmctl(shMemId, IPC_RMID, NULL);
    }

    return 0;
}

