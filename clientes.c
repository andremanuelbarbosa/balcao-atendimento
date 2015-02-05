/*********************************************/
/*     Librarias necessarias ao programa     */
/*********************************************/
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <time.h>
/*********************************************/


/***************************************/
/*     Variveis finais do programa     */
/***************************************/
#define MIN_WAIT_TIME 1
#define MAX_WAIT_TIME 100
#define MIN_NUM_TASKS 1
#define MAX_NUM_TASKS 10
/***************************************/


/*********************************************/
/*     Prototipos das funcoes existentes     */
/*********************************************/
void handler(void);
void sig_usr(int);
/*********************************************/


/*****************************************/
/*     Variaveis globais do programa     */
/*****************************************/
int num_clientes;
int num_signals = 0;
/*****************************************/


/*****************************/
/*     Programa CLIENTES     */
/*****************************/
int main(int argc, char *argv[])
{
	int i;
	int tempo_med;
	int rand_num, signal;
	float num, tempo_int;
	struct timespec interval, remain, total_time;
	pid_t pid1,pid2;
	int wait_time, num_tasks;
	char *command;

	system("clear");

	//----------------------------------------------------------------
	// Tratamento dos parametros
	//----------------------------------------------------------------
	if(argc != 3)
	{
		printf("Numero de parametros invalidos!!!\n");
		exit(2);
	}

	if(sscanf(argv[1],"%d",&num_clientes) <= 0)
	{
		printf("O parametro com o numero de clientes e invalido!!!\n");
		exit(2);
	}

	if(sscanf(argv[2],"%d",&tempo_med) <= 0)
	{
		printf("O parametro com o tempo medio de intervalo e invalido!!!\n");
		exit(2);
	}
	//----------------------------------------------------------------

	total_time.tv_sec = 0;
	total_time.tv_nsec = 0;

	pid1 = getpid();

	for(i = 0; i < num_clientes; i++)
	{
		pid2 = getpid();

		if(pid2 == pid1)
		{
			rand_num = (random() % 20) + 1;
			num = ((float) rand_num) / 100;
			signal = (random() % 2);

			if(signal)
			{
				tempo_int = tempo_med - (tempo_med * num);
				interval.tv_nsec = (100 - rand_num) * 10000000;
			}
			else
			{
				tempo_int = tempo_med + (tempo_med * num);
				interval.tv_nsec = rand_num * 10000000;
			}
			interval.tv_sec = (int) tempo_int;

			total_time.tv_sec += interval.tv_sec;
			if((total_time.tv_nsec + interval.tv_nsec) > 999999999)
			{
				total_time.tv_sec++;
				total_time.tv_nsec = (total_time.tv_nsec + interval.tv_nsec) - 1000000000;
			}
			else
				total_time.tv_nsec += interval.tv_nsec;

			printf("\t%d\t%d\t%d\t%d\t%d\n",rand_num,signal,interval.tv_sec,interval.tv_nsec,total_time.tv_nsec);
			if(fork() == 0)
			{
				wait_time = (random() % (MAX_WAIT_TIME - MIN_WAIT_TIME)) + MIN_WAIT_TIME + 1;
				num_tasks = (random() % (MAX_NUM_TASKS - MIN_NUM_TASKS)) + MIN_NUM_TASKS + 1;
				command = (char *) malloc((sizeof(char) * 9) + (sizeof(int) * 2));
				sprintf(command,"cliente %d %d",wait_time,num_tasks);
				system(command);
				kill(getppid(),SIGUSR1);
				free(command);
				exit(0);
			}
			else
				nanosleep(&interval,&remain);
		}
	}

	return 0;
}
/*****************************/
