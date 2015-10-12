#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <semaphore.h>


#define MAX_PROC 5

int main()
{
	//Массив для сортировки
	int array[] = {11, 42, 2, 145, 33, 60, 25, 28, 8, 7, 49, 12, 24, -1, 15, 91, 20, 29, 2, 20};
	int proc_count = MAX_PROC;

	//Выделение shared memory
	int memsize = sizeof(array)/sizeof(array[0]);
	int size_of_shmem = (sizeof(array[0])*memsize)+sizeof(proc_count);
	int shmem_id = shmget(IPC_PRIVATE, size_of_shmem, SHM_W | SHM_R | IPC_CREAT);
	if(shmem_id < 0)
	{
		perror("shmget error!\n");
		exit(EXIT_FAILURE);
	}
	
	//Привязка shmem к процессу
	int *shmem = (int *)shmat(shmem_id, NULL, 0);

	//Счётчик процессов - первая "ячейка" shmem
	shmem[0] = proc_count;
	//Копирование массива для сортировки в shmem
	for(int i = 1; i < size_of_shmem/sizeof(shmem[0]); i++)
	{
		shmem[i] = array[i-1];
	}

	//Первоначальный массив в shmem
	printf("Первоначальный массив: \n");
	for(int i = 1; i < size_of_shmem/sizeof(shmem[0]); i++)
	{
		printf("%d ", shmem[i]);
	}
	printf("\n");

	//Создание семафора
	sem_t *sem = sem_open("qs_sem", O_CREAT | O_EXCL, 0644, 1);
	if(sem == SEM_FAILED)
	{
		perror("sem_open error!\n");
		exit(EXIT_FAILURE);
	}
	sem_unlink("qs_sem");
	
/////////////////////////////////////////////////////////////////////////////////

	printf("Главный процесс: %d\n", getpid());
	pid_t quick_sort_proc_id = fork();

	//Первый дочерний процесс, сортировка запускается в нём
	if(quick_sort_proc_id == 0)
	{
		char shmem_id_arg[10];
		char l_arg[4];
		char r_arg[4];
		if(sprintf(shmem_id_arg, "%d", shmem_id) < 0 || sprintf(l_arg, "%d", 1) < 0 || sprintf(r_arg, "%d", (size_of_shmem/sizeof(shmem[0]))-1) < 0)
		{
			printf("Error sprintf\n");
		}
		execl("qsort" , shmem_id_arg, l_arg, r_arg, NULL);
	}

/////////////////////////////////////////////////////////////////////////////////

	//Продолжение работы главного процесса
	else
	{
		waitpid(quick_sort_proc_id, NULL, 0);
		printf("Main stop waiting for qsort\n");
		//Массив после сортировки
		printf("Массив после сортировки: \n");
		for(int i = 1; i < size_of_shmem/sizeof(shmem[0]); i++)
		{
			printf("%d ", shmem[i]);
		}
		printf("\n");

		//Уничтожение семафора
		sem_destroy(sem);

		//Отсоединение памяти и удаление сегмента
		shmdt(shmem);
		shmctl(shmem_id, IPC_RMID, NULL);
	}

	return 0;
}
