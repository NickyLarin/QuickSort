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

void quickSort(int *array, int first, int last, sem_t *sem);
pid_t createNewProcess(int *proc_count, sem_t *sem);

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
	pid_t quick_sort_proc_id = createNewProcess(&(shmem[0]), sem);

	//Начало сортировки в дочернем процессе
	if(!quick_sort_proc_id)
	{
		printf("Первый дочерний процесс: %d\n", getpid());
		quickSort(shmem, 1, (size_of_shmem/sizeof(shmem[0]))-1, sem);
	}

/////////////////////////////////////////////////////////////////////////////////

	//Продолжение работы главного процесса
	else
	{
		waitpid(quick_sort_proc_id, NULL, 0);

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

//Создание нового процесса
//При создании счётчик процессов уменьшается на 1 дочерним процессом
//Доступ к счётчику контролируется семафором
pid_t createNewProcess(int *proc_count, sem_t *sem)
{
	sem_wait(sem);
	if(*proc_count > 0)
	{
		pid_t new_process_id = fork();
		if(new_process_id > 0)
		{
			*proc_count -= 1;
			sem_post(sem);
			return new_process_id;
		}
		else if(new_process_id == 0)
		{
			printf("Процесс %d создан\n", getpid());
			return (pid_t)0;
		}
		else
		{
			perror("New process fork error!");
			exit(EXIT_FAILURE);
		}
	}
	else
	{
		sem_post(sem);
		return (pid_t)-1;
	}
}

void swap(int *a, int *b)
{
	int temp = *a;
	*a = *b;
	*b = temp;
}


void quickSort(int *array, int first, int last, sem_t *sem)
{
	sem_wait(sem);
	printf("Процесс: %d proc_count %d first: %d last: %d\n", getpid(), array[0], first, last);
	sem_post(sem);
	int l = first;
	int r = last;
	int pivot = array[(first + last) / 2];
	
	do
	{
		while(array[l] < pivot)
		{
			l++;
		}
		while(array[r] > pivot)
		{
			r--;
		}
	
		if(l <= r)
		{
			if(array[l] >= array[r])
			{
				swap(&array[l], &array[r]);
				l++;
				r--;
			}
		}
	}
	while(l <= r);
	
	//Создаём дочерний процесс
	pid_t new_process;
	
	//Если мы в дочернем процессе или кол-во процессов уже максимальное,
	//то вызываем сортировку для левой части массиво
	if(l < last)
	{
		new_process = createNewProcess(&(array[0]), sem);
		if(new_process == 0 || new_process == -1)
		{
			quickSort(array, l, last, sem);
		}
	}
	//В родительском процессе вызываем сортировку для правой части массива
	if(first < r)
	{
		if(new_process > 0 || new_process == -1)
		{
			quickSort(array, first, r, sem);
		}
	}
	if(new_process > 0)
	{
		waitpid(new_process, NULL, 0);
	}
}
