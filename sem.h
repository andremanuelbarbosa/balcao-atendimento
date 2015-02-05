
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/types.h>

#define SEM_A 0200
#define SEM_R 0400
#define SEM_MODE (SEM_R | SEM_A)


static struct sembuf op_arr[3] = {0,0,0};


/***************************************************************************************************/
/*     Cria um semaforo com a chave key e valor inicial init_val e retorna a sua identificacao     */
/***************************************************************************************************/
int sem_create(key_t key, int init_val)
{
	int id;
	union semun 
	{
		int val;
		struct semid_ds *buf;
		ushort *array;
	} semctl_arg;

	if((id = semget(key,1,IPC_CREAT|IPC_EXCL|SEM_MODE)) < 0)
		fprintf(stderr,"Error: unable to create semaphore\n");
	semctl_arg.val = init_val;

	if(semctl(id,0,SETVAL,semctl_arg) < 0)
		fprintf(stderr,"Error: unable to initialize semaphore\n");

	return(id);
}
/***************************************************************************************************/


/****************************************************************/
/*     Espera ate obter o controlo dos recursos disponiveis     */
/****************************************************************/
void sem_wait(int id)
{
	op_arr[0].sem_op = -1;
	if(semop(id,&op_arr[0],1) < 0)
		fprintf(stderr,"Error: unable to wait for control of resources\n");
}
/****************************************************************/


/********************************************************/
/*     Devolve o controlo dos recursos ao programa      */
/********************************************************/
void sem_signal(int id)
{
	op_arr[0].sem_op = 1;
	if(semop(id,&op_arr[0],1) < 0)
		fprintf(stderr,"Error: unable to return control of resources to program\n");
}
/********************************************************/


/****************************************************/
/*     Remove o semaforo com a identificacao id     */
/****************************************************/
int sem_remove(int id)
{
	if(semctl(id,0,IPC_RMID,0) < 0)
	{
		fprintf(stderr,"Error: unable to remove semaphore\n");
		return -1;
	}

	return 0;
}
/****************************************************/

int read_line(int fd, char *str)
{
	int n;

	do
	{
		n = read(fd,str,1);
	}
	while(n > 0 && *str++ != '\0');
	return (n > 0);
}
