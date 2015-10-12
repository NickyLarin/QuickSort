#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/ipc.h>

#define MAX_R_CALLS 5

void quickSort(int *array, int first, int last);

int main(int argc, char *argv[])
{
	//Парсим переданные как параметры данные, привязываем shared memory
	int *array;
	int l, r;
	if(argc < 1)
	{
		printf("Error: No args\nProcess id: %d\n", getpid());
		exit(EXIT_FAILURE);
	}
	else if(argc < 3)
	{
		printf("Error: Too few arguments\nProcess id: %d\n", getpid());
		exit(EXIT_FAILURE);
	}
	else
	{
		if((array = (int *)shmat(atoi(argv[0]), NULL, 0))==(int *)(-1))
		{
			printf("Error: shmat error\nProcess id: %d\n", getpid());
			exit(EXIT_FAILURE);
		}
		else
		{
			l = atoi(argv[1]);
			r = atoi(argv[2]);
		}
	}
	//Начинаем сортировку
	quickSort(array, l, r);

	//Отсоединяем память
	shmdt(array);

	return 0;
}


void swap(int *a, int *b)
{
	int temp = *a;
	*a = *b;
	*b = temp;
}


//Простая быстрая сортировка
void quickSort(int *array, int first, int last)
{
	static int count = 0;
	count++;
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


	if(l < last)
	{
		quickSort(array, l, last);
	}
	if(first < r)
	{
		quickSort(array, first, r);
	}
}

void createNewProc(int shmem_id, int l, int r)
{
	pid_t new_process = fork();
	if(!new_process)
	{
		char shmem_id_arg[10];
		char l_arg[4];
		char r_arg[4];
		if(
		sprintf(shmem_id_arg, "%d", shmem_id) < 0 || 
		sprintf(l_arg, "%d", l) < 0 || 
		sprintf(r_arg, "%d", r)-1) < 0)
		{
			printf("Error sprintf\n");
			exit(EXIT_FAILURE)
		}
		execl("qsort" , shmem_id_arg, l_arg, r_arg, NULL);
	}
}
