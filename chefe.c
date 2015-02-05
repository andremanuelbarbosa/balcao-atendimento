/*********************************************/
/*     Librarias necessarias ao programa     */
/*********************************************/
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include "sem.h"
/*********************************************/


/*********************************************/
/*     Prototipos das funcoes existentes     */
/*********************************************/
void *detect_clients(void *);
static void clean(void);
/*********************************************/


/*****************************************/
/*     Variaveis globais do programa     */
/*****************************************/
int sem_id; //id do semaforo
int tickets_counter; //id da regiao de memoria do contador
int tickets_panel; //id da regiao de memoria do painel

int num_workers;
int num_clients_worked;
int num_clients_detected;

int end = 0;
int end_workers = 0;
int end_clients = 0;

int main_pid; //pid do processo principal
int detect_clients_ready = 0;

int free_worker_flag = 0;

FILE *file;
/*****************************************/


/*************************/
/*     Programa CHEFE    */
/*************************/
int main(int argc, char *argv[])
{
	int i;
	int fd;
	char *fifo_name;

	int service_time;

	pthread_t clients_thread;
	pthread_t update_thread;

	pid_t *workers_pids;
	pid_t *clients_pids;

	time_t start_time;
	time_t actual_time;
	double diff_time;

	pid_t pid1,pid2;
	char *command;

	system("clear");
	main_pid = getpid();

	//----------------------------------------------------------------
	// Tratamento dos parametros
	//----------------------------------------------------------------
	if(argc != 3)
	{
		printf("CHEFE - Numero de parametros invalidos!!!\n");
		exit(2);
	}

	if(sscanf(argv[1],"%d",&num_workers) <= 0)
	{
		printf("CHEFE - O parametro com o numero de caixas e invalido!!!\n");
		exit(2);
	}

	num_clients_worked = 0;
	num_clients_detected = 0;

	if(sscanf(argv[2],"%d",&service_time) <= 0)
	{
		printf("CHEFE - O parametro com a duracao do servico e invalido!!!\n");
		exit(2);
	}
	//----------------------------------------------------------------


	//----------------------------------------------------------------
	// Instalacao da rotina de saida
	//----------------------------------------------------------------
	if(atexit(clean))
	{
		printf("CHEFE - Erro na instalacao da rotina de saida no chefe %d!!!\n",getpid());
		exit(2);
	}
	//----------------------------------------------------------------


	workers_pids = (pid_t *) malloc(sizeof(pid_t) * num_workers);


	//----------------------------------------------------------------
	// Criacao das regioes de memoria e dos semaforos
	//----------------------------------------------------------------
	if((tickets_counter = shmget(ftok("cliente",0),sizeof(int),IPC_CREAT | 0777)) == -1)
	{
		printf("CHEFE - Erro na criacao da regiao de memoria partilhada das senhas!!!\n");
		exit(2);
	}

	if((tickets_panel = shmget(ftok("cliente",1),(sizeof(int) + sizeof(pid_t)),IPC_CREAT | 0777)) == -1)
	{
		printf("CHEFE - Erro na criacao da regiao de memoria parilhada do painel!!!\n");
		exit(2);
	}

	if((sem_id = sem_create(getpid(),1)) == -1)
	{
		printf("CHEFE - Erro na criacao do semaforo\n");
		exit(2);
	}
	//----------------------------------------------------------------


	//----------------------------------------------------------------
	// Criacao do ficheiro work.log
	//----------------------------------------------------------------
	if((file = fopen("work.log","w")) == NULL)
	{
		printf("CHEFE - Erro na criacao do ficheiro work.log!!!\n");
		exit(2);
	}
	fclose(file);
	//----------------------------------------------------------------


	//----------------------------------------------------------------
	// Lançamento da thread detect_clients
	//----------------------------------------------------------------
	pthread_create(&clients_thread,NULL,detect_clients,&tickets_counter);
	//----------------------------------------------------------------


	//----------------------------------------------------------------
	// Lançamento dos processos caixa
	//----------------------------------------------------------------
	printf("\n");
	printf("CHEFE - A lancar os processos caixa...\n");
	pid1 = getpid();

	for(i = 0; i < num_workers; i++)
	{
		pid2 = getpid();

		if(pid2 == pid1)
		{
			sleep(1);
			if((workers_pids[i] = fork()) == 0)
			{
				system("caixa 1 20 2");
				exit(0);
			}
		}
	}
	printf("CHEFE - Processos caixa lancados\n");
	//----------------------------------------------------------------


	//----------------------------------------------------------------
	// Testa o tempo de serviço
	//----------------------------------------------------------------
	sleep(service_time);
	//----------------------------------------------------------------


	//----------------------------------------------------------------
	// Indica a thread detect_clients que pode terminar
	//----------------------------------------------------------------
	printf("\n");
	printf("CHEFE - A terminar a thread detect_clients...\n");
	sem_wait(sem_id);
	end = 1;
	sem_signal(sem_id);

	sem_wait(sem_id);
	fd = open("aux_fifo",O_WRONLY);
	sem_signal(sem_id);
	usleep(100000);
	close(fd);
	pthread_join(clients_thread,NULL);
	printf("CHEFE - Thread detect_clients terminada\n");
	//----------------------------------------------------------------


	//----------------------------------------------------------------
	// Indica as caixas que o tempo de servico terminou
	//----------------------------------------------------------------
	printf("\n");
	printf("CHEFE - A indicar as caixas que o tempo de servico terminou...\n");
	for(i = 0; i < num_workers; i++)
	{
		fifo_name = (char *) malloc((sizeof(char) * 8) + sizeof(pid_t) + 1);
		sprintf(fifo_name,"caixa%daux",workers_pids[i]);
		printf("CHEFE - Vai tentar abrir FIFO %s\n",fifo_name);
		fd = open(fifo_name,O_WRONLY);
		printf("CHEFE - FIFO %s aberto para escrita\n",fifo_name);
		close(fd);
		free(fifo_name);
		sleep(1);
	}
	printf("CHEFE - Caixas ja sabem que o tempo de servico terminou\n");
	//----------------------------------------------------------------


	//----------------------------------------------------------------
	// Espera que as caixas e os clientes terminem
	//----------------------------------------------------------------
	printf("\n");
	printf("CHEFE - A espera que as caixas terminem...\n");
	for(i = 0; i < num_workers; i++)
	{
		waitpid(workers_pids[i],NULL,0);
		printf("CHEFE - Fim do processo caixa com PID = %d\n",workers_pids[i]);
	}
	printf("CHEFE - Todos os processos caixa terminaram!!!\n");
	//----------------------------------------------------------------


	printf("CHEFE - Fim do processo\n");

	return 0;
}
/*************************/


/****************************************************/
/*     Thread que detecta a chegada de clientes     */
/****************************************************/
void *detect_clients(void *shmid)
{
	int fd;
	int *adr_shmid;
	char buffer[10];

	adr_shmid = (int *) shmat(*(int *)shmid,0,SHM_RND);
	mkfifo("aux_fifo",0777);

	sem_wait(sem_id);
	(*adr_shmid)++;
	sem_signal(sem_id);

	sem_wait(sem_id);
	detect_clients_ready = 1;
	sem_signal(sem_id);

	while(1)
	{
		fd = open("aux_fifo",O_RDONLY);

		sem_wait(sem_id);
		if(end)
		{
			sem_signal(sem_id);
			break;
		}
		sem_signal(sem_id);

		read_line(fd,buffer);
		fflush(stdin);
		close(fd);
		printf("CHEFE - Chegou cliente com PID %s\n",buffer);

		sem_wait(sem_id);
		num_clients_detected++;
		sem_signal(sem_id);

		(*adr_shmid)++;
		printf("CHEFE - A indicar ao cliente %s que detectou a sua chegada...\n",buffer);
		fd = open("aux_fifo",O_WRONLY);
		close(fd);
	}

	printf("CHEFE - Fim da thread detect_clients\n");

	unlink("aux_fifo");
	shmdt(adr_shmid);
}
/****************************************************/


/****************************************************/
/*     Limpa os recursos utilizados no processo     */
/****************************************************/
static void clean(void)
{
	if(getpid() != main_pid)
		exit(0);

	printf("CHEFE - Numero de trabalhadores inicializados: %d\n",num_workers);
	printf("CHEFE - Numero de clientes detectados: %d\n",num_clients_detected);
	printf("CHEFE - Numero de clientes atendidos: %d\n",num_clients_worked);

	printf("CHEFE - A limpar recursos utilizados...\n");
	sem_remove(sem_id);
	shmctl(tickets_counter,IPC_RMID,NULL);
	shmctl(tickets_panel,IPC_RMID,NULL);
	printf("CHEFE - Recursos utilizados limpos\n");
}
/****************************************************/
