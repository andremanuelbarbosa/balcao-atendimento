/*********************************************/
/*     Librarias necessarias ao programa     */
/*********************************************/
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <string.h>
#include <time.h>
#include "sem.h"
/*********************************************/


/*********************************************/
/*     Prototipos das funcoes existentes     */
/*********************************************/
void write_log(char *);
static void clean(void);
char *generate_operation(void);
/*********************************************/


/*****************************************/
/*     Variaveis globais do programa     */
/*****************************************/
int sem_id;
int main_pid;

char *fifo_client_name;
char *fifo_worker_name;

FILE *log_file_fd;
char *log_file_name;
/*****************************************/


/****************************/
/*     Programa CLIENTE     */
/****************************/
int main(int argc, char *argv[])
{
	int i;
	int fd;
	int fd_client;
	int fd_worker;
	char message[10];

	int wait_time, num_tasks;

	char client_pid[10];

	char *task_aux;
	char tasks[10][50];

	int seconds = 0;
	int client_succeded = 0;

	int ticket_client;
	int ticket_panel;

	int tickets_counter;
	int *tickets_counter_address;

	int tickets_panel;
	int panel_worker;
	int *tickets_panel_address;

	char worker_message[20];

	char line[100];

	main_pid = getpid();


	//----------------------------------------------------------------
	// Tratamento dos parametros
	//----------------------------------------------------------------
	if(argc != 3)
	{
		printf("Numero de parametros invalidos!!!\n");
		exit(2);
	}

	if(sscanf(argv[1],"%d",&wait_time) <= 0)
	{
		printf("O parametro com o tempo maximo e invalido!!!\n");
		exit(2);
	}

	if(sscanf(argv[2],"%d",&num_tasks) <= 0)
	{
		printf("O parametro com o numero de operacoes e invalido!!!\n");
		exit(2);
	}
	//----------------------------------------------------------------


	//----------------------------------------------------------------
	// Instalacao da rotina de saida
	//----------------------------------------------------------------
	if(atexit(clean))
	{
		printf("Erro na instalacao da rotina de saida no cliente %d!!!\n",getpid());
		exit(2);
	}
	//----------------------------------------------------------------


	//----------------------------------------------------------------
	// Obtencao das chaves para as regioes de memoria partilhadas
	//----------------------------------------------------------------
	if((tickets_counter = shmget(ftok("cliente",0),sizeof(int),SHM_R)) == -1)
	{
		printf("Erro na obtencao da chave da regiao de memoria partilhada das senhas!!!\n");
		exit(2);
	}

	if((tickets_panel = shmget(ftok("cliente",1),(sizeof(int) + sizeof(pid_t)),SHM_R)) == -1)
	{
		printf("Erro na obtencao da chave da regiao de memoria partilhada do painel!!!\n");
		exit(2);
	}
	//----------------------------------------------------------------


	//----------------------------------------------------------------
	// Criacao do semaforo
	//----------------------------------------------------------------
	if((sem_id = sem_create(getpid(),1)) == -1)
	{
		printf("Erro na criacao do semaforo\n");
		exit(2);
	}
	//----------------------------------------------------------------


	//----------------------------------------------------------------
	// Criaca do ficheiro clientePID.log
	//----------------------------------------------------------------
	log_file_name = (char *) malloc((sizeof(char) * 11) + sizeof(pid_t));
	sprintf(log_file_name,"cliente%d.log",getpid());
	if((log_file_fd = fopen(log_file_name,"w")) == NULL)
	{
		printf("CLIENTE %d - Erro na criacao do ficheiro %s!!!\n",getpid(),log_file_name);
		exit(2);
	}
	//----------------------------------------------------------------


	sprintf(line,"Inicio do processo");
	write_log(line);


	//----------------------------------------------------------------
	// Cliente "retira" uma senha
	//----------------------------------------------------------------
	tickets_counter_address = (int *) shmat(tickets_counter,0,SHM_RND);
	sem_wait(sem_id);
	ticket_client = *tickets_counter_address;
	printf("Valor da senha no contador: %d\n",*tickets_counter_address);
	sem_signal(sem_id);
	shmdt(tickets_counter_address);
	//----------------------------------------------------------------


	sprintf(line,"Retirou a senha %d",ticket_client);
	write_log(line);


	//----------------------------------------------------------------
	// Indica ao processo chefe que chegou um cliente
	//----------------------------------------------------------------
	if((fd = open("aux_fifo",O_WRONLY)) == -1)
	{
		printf("Erro na abertura do fifo aux_fifo para escrita1!!!\n");
		exit(2);
	}

	sprintf(message,"%d\n",getppid());
	write(fd,message,strlen(message) + 1);
	fflush(stdout);
	close(fd);

	if((fd = open("aux_fifo",O_RDONLY)) == -1)
	{
		printf("Erro na abertura do fifo para leitura!!!\n");
		exit(2);
	}
	close(fd);
	//----------------------------------------------------------------


	//----------------------------------------------------------------
	// Cliente fica a espera da sua vez ate ao tempo maximo de espera
	//----------------------------------------------------------------
	while(wait_time > seconds)
	{
		tickets_panel_address = (int *) shmat(tickets_panel,0,SHM_RND);
		sem_wait(sem_id);
		ticket_panel = *tickets_panel_address;
		sem_signal(sem_id);
		printf("CLIENTE %d - Valor da senha no painel: %d\n",getpid(),*tickets_panel_address);
		shmdt(tickets_panel_address);

		sprintf(line,"Olhou para o painel: %d",ticket_panel);
		write_log(line);

		if(ticket_panel == ticket_client)
		{
			printf("CLIENTE %d - Chegou a minha vez\n",getpid());
			sprintf(line,"Chegou a minha vez");
			write_log(line);

			fifo_client_name = (char *) malloc((sizeof(char) * 11) + sizeof(pid_t));
			sprintf(fifo_client_name,"FIFOcliente%d",getpid());
			mkfifo(fifo_client_name,0777);

			tickets_panel_address = (int *) shmat(tickets_panel,0,SHM_RND);
			sem_wait(sem_id);
			panel_worker = *(tickets_panel_address + 1);
			sem_signal(sem_id);
			shmdt(tickets_panel_address);

			sprintf(line,"A dirigir-se para a caixa: %d",panel_worker);
			write_log(line);

			fifo_worker_name = (char *) malloc((sizeof(char) * 9) + sizeof(pid_t));
			sprintf(fifo_worker_name,"FIFOcaixa%d",panel_worker);
			sprintf(client_pid,"%d",getpid());
			fd_worker = open(fifo_worker_name,O_WRONLY);
			printf("CLIENTE %d - Mensagem com o pid: %s\n",getpid(),client_pid);
			write(fd_worker,client_pid,strlen(client_pid) + 1);
			fflush(stdout);
			close(fd_worker);

			sprintf(line,"Chegou ao balcao da caixa: %d",panel_worker);
			write_log(line);

			sleep(3);

			for(i = 0; i < num_tasks; i++)
			{
				task_aux = generate_operation();

				sprintf(line,"A pedir a operacao %s",task_aux);
				write_log(line);

				fd_worker = open(fifo_worker_name,O_WRONLY);
				write(fd_worker,task_aux,strlen(task_aux) + 1);
				fflush(stdout);
				close(fd_worker);

				sprintf(line,"Operacao %s pedida",task_aux);
				write_log(line);

				sleep(1);

				fd = open(fifo_client_name,O_RDONLY);
				usleep(100000);
				read_line(fd,worker_message);
				close(fd);

				printf("CLIENTE %d - Operacao executada\n",getpid());
				sprintf(line,"Operacao %s executada pela caixa %d",task_aux,panel_worker);
				write_log(line);
			}

			sprintf(line,"A informar a caixa que ja nao tem mais pedidos para fazer");
			write_log(line);

			task_aux = "END";
			fd_worker = open(fifo_worker_name,O_WRONLY);
			write(fd,task_aux,strlen(task_aux) + 1);
			fflush(stdout);
			close(fd);

			fd = open(fifo_client_name,O_RDONLY);
			usleep(100000);
			close(fd);

			sprintf(line,"A sair do balcao");
			write_log(line);

			client_succeded = 1;
			break;
		}
		sleep(1);
		seconds++;
	}
	//----------------------------------------------------------------


	if(!client_succeded)
	{
		printf("CLIENTE %d - Abandonou o servico de atendimento porque nao pode esperar mais\n",getpid());
		sprintf(line,"Abandonou o servico de atendimento porque nao pode esperar mais");
		write_log(line);
	}

	sprintf(line,"Fim do processo");
	write_log(line); 

	return 0;
}
/****************************/


/******************************************************************/
/*     Escreve para o ficheiro clientePID.log as suas tarefas     */
/******************************************************************/
void write_log(char *str)
{
	char line[100];

	time_t actual_time_;
	struct tm *actual_time;

	// Devolve o tempo actual e coloca na estrutura
	time(&actual_time_);
	actual_time = gmtime(&actual_time_);

	// Escreve no ficheiro a tarefa
	sprintf(line,"%d:%.02d:%.02d - %s\n",actual_time->tm_hour,actual_time->tm_min,actual_time->tm_sec,str);
	fwrite(line,strlen(line),1,log_file_fd);
}
/******************************************************************/


/****************************************************/
/*     Limpa os recursos utilizados no processo     */
/****************************************************/
static void clean(void)
{
	if(getpid() != main_pid)
		exit(0);

	printf("A limpar recursos utilizados na cliente %d... \n",getpid());
	sem_remove(sem_id);
	unlink(fifo_client_name);
	free(fifo_client_name);
	free(fifo_worker_name);
	free(log_file_name);
	fclose(log_file_fd);
	printf("Recursos utlizados limpos na cliente\n");
}
/****************************************************/


/*********************************************************************/
/*     Gera um tipo de operacao que o cliente ira pedir ao caixa     */
/*********************************************************************/
char *generate_operation(void)
{
	int i;
	int index;
	int letter;
	char *buf;

	index = (random() % 50) + 1;
 	buf = (char *) malloc((sizeof(char) * index) + 1);

	for(i = 0; i < index; i++)
	{
		letter = (random() % 26) + 97;
		buf[i] = (char) letter;
	}
	buf[index] = '\0';

	//free(buf);
	return buf;
}
/*********************************************************************/
