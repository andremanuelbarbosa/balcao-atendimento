/*********************************************/
/*     Librarias necessarias ao programa     */
/*********************************************/
#include <stdio.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <pthread.h>
#include <string.h>
#include "sem.h"
/*********************************************/


/*********************************************/
/*     Prototipos das funcoes existentes     */
/*********************************************/
void write_log(char *);
void write_log_aux(FILE *, char *);
static void clean(void);
void *detect_service_end(void *);
/*********************************************/


/*****************************************/
/*     Variaveis globais do programa     */
/*****************************************/
int sem_id;
int sem_id_thread;

char *fifo_name;
char *fifo_name_thread;

char *fifo_client_name;

int main_pid;
int service_end = 0;
int client_arrived = 0;

FILE *log_file_fd;
char *log_file_name;
/*****************************************/


/**************************/
/*     Programa CAIXA     */
/**************************/
int main(int argc, char *argv[])
{
	char *message;
	char client_pid[10];

	char buffer[10];
	char operation[100];

	int panel_visible_time;
	int max_wait_time;
	int operation_base_time;

	pid_t father_pid;
	char *fifo_name_aux;

	int service_time = 0;
	int time_counter = 0;

	int rand_num;
	float num;
	int signal;
	long int operation_wait_time;

	pthread_t service_end_thread;

	int fd;

	int ticket_counter;
	int tickets_counter;
	int *tickets_counter_address;

	int ticket_panel;
	int tickets_panel;
	int *tickets_panel_address;

	int worked_clients = 0;
	char *client_messages[] = {"Executado","Adiado","Impossivel"};

	char line[100];
	FILE *file_work;

	main_pid = getpid();

	//----------------------------------------------------------------
	// Tratamento dos parametros
	//----------------------------------------------------------------
	if(argc != 4)
	{
		printf("CAIXA %d - Numero de parametros invalidos!!!\n",getpid());
		exit(2);
	}

	if(sscanf(argv[1],"%d",&panel_visible_time) <= 0)
	{
		printf("CAIXA %d - O parametro com o tempo de visibilidade do painel e invalido!!!\n",getpid());
		exit(2);
	}

	if(sscanf(argv[2],"%d",&max_wait_time) <= 0)
	{
		printf("CAIXA %d - O parametro com o tempo maximo de espera por um cliente e invalido!!!\n",getpid());
		exit(2);
	}

	if(sscanf(argv[3],"%d",&operation_base_time) <= 0)
	{
		printf("CAIXA %d - O parametro com o tempo base de uma operacao e invalido!!!\n",getpid());
		exit(2);
	}
	//----------------------------------------------------------------


	//----------------------------------------------------------------
	// Instalacao da rotina de saida
	//----------------------------------------------------------------
	if(atexit(clean))
	{
		printf("CAIXA %d - Erro na instalacao da rotina de saida!!!\n",getpid());
		exit(2);
	}
	//----------------------------------------------------------------


	//----------------------------------------------------------------
	// Obtencao das chaves para as regioes de memoria do contador e do painel
	//----------------------------------------------------------------
	if((tickets_counter = shmget(ftok("cliente",0),sizeof(int),SHM_R)) == -1)
	{
		printf("CAIXA %d - Erro na obtencao da chave da regiao de memoria partilhada do contador!!!\n",getpid());
		exit(2);
	}
	if((tickets_panel = shmget(ftok("cliente",1),(sizeof(int) + sizeof(pid_t)),SHM_W)) == -1)
	{
		printf("CAIXA %d - Erro na obtencao da chave da regiao de memoria partilhada do painel!!!\n",getpid());
		exit(2);
	}
	//----------------------------------------------------------------


	//----------------------------------------------------------------
	// Criacao do semaforo
	//----------------------------------------------------------------
	if((sem_id = sem_create(getpid(),1)) == -1)
	{
		printf("CAIXA %d - Erro na criacao do semaforo\n",getpid());
		exit(2);
	}
	//----------------------------------------------------------------


	//----------------------------------------------------------------
	// Criacao do FIFO a usar para a comunicaçao com o cliente
	//----------------------------------------------------------------
	fifo_name = (char *) malloc((sizeof(char) * 9) + sizeof(pid_t));
	sprintf(fifo_name,"FIFOcaixa%d",getppid());

	if(mkfifo(fifo_name,0777) == -1)
	{
		printf("CAIXA %d - Erro na criacao do fifo %s!!!\n",getpid(),fifo_name);
		exit(2);
	}
	//----------------------------------------------------------------


	//----------------------------------------------------------------
	// Criaca do ficheiro caixaPID.log
	//----------------------------------------------------------------
	log_file_name = (char *) malloc((sizeof(char) * 9) + sizeof(pid_t));
	sprintf(log_file_name,"caixa%d.log",getppid());
	if((log_file_fd = fopen(log_file_name,"w")) == NULL)
	{
		printf("CAIXA %d - Erro na criacao do ficheiro %s!!!\n",getpid(),log_file_name);
		exit(2);
	}
	//----------------------------------------------------------------


	sprintf(line,"Inicio do funcionamento");
	write_log(line);

	sem_wait(sem_id);
	file_work = fopen("work.log","a+");
	write_log_aux(file_work,line);
	fclose(file_work);
	sem_signal(sem_id);

	father_pid = getppid();
	pthread_create(&service_end_thread,NULL,detect_service_end,&father_pid);

	while(1)
	{
		// Verifica se ha clientes para atender
		sem_wait(sem_id);
		tickets_counter_address = (int *) shmat(tickets_counter,0,SHM_RND);
		tickets_panel_address = (int *) shmat(tickets_panel,0,SHM_RND);
		ticket_counter = *tickets_counter_address;
		ticket_panel = *tickets_panel_address;
		shmdt(tickets_panel_address);
		shmdt(tickets_counter_address);
		sem_signal(sem_id);

		if((ticket_counter - 1) > ticket_panel) // Ha clientes para atender
		{
			printf("CAIXA %d - CHEGOU CLIENTE!!!\n",getpid());
			// Actualiza o painel
			sem_wait(sem_id);
			tickets_panel_address = (int *) shmat(tickets_panel,0,SHM_RND);
			(*tickets_panel_address)++;
			ticket_panel = *tickets_panel_address;
			*(tickets_panel_address + 1) = getppid();
			shmdt(tickets_panel_address);
			sem_signal(sem_id);

			sprintf(line,"Actuacao sobre o painel: senha %d e caixa %d",ticket_panel,getppid());
			write_log(line);

			sem_wait(sem_id);
			file_work = fopen("work.log","a+");
			write_log_aux(file_work,line);
			fclose(file_work);
			sem_signal(sem_id);

			sleep(panel_visible_time);

			sprintf(line,"A espera que o cliente se encaminhe ao balcao");
			write_log(line);

			sem_wait(sem_id);
			file_work = fopen("work.log","a+");
			write_log_aux(file_work,line);
			fclose(file_work);
			sem_signal(sem_id);

			// Espera que apareça o cliento ou termina se passar o tempo maximo d espera
			time_counter = 0;
			while(max_wait_time > time_counter)
			{
				fd = open(fifo_name,O_RDONLY | O_NONBLOCK);
				usleep(100000);
				if(read_line(fd,client_pid) > 0)
				{
					fflush(stdin);
					printf("CAIXA %d - Conseguiu abrir o FIFO %d e o cliente tem pid %s\n",getpid(),fd,client_pid);
					close(fd);

					sprintf(line,"Inicio do atendimento do cliente %s",client_pid);
					write_log(line);

					sem_wait(sem_id);
					file_work = fopen("work.log","a+");
					write_log_aux(file_work,line);
					fclose(file_work);
					sem_signal(sem_id);

					break;
				}
				fflush(stdin);
				close(fd);
				time_counter++;
				sleep(1);
			}

			//free(client_pid);

			if(max_wait_time <= time_counter) // O cliente nao apareceu durante o tempo maximo de espera
			{
				printf("CAIXA %d - O cliente %s com senha %d nao se dirigiu ao balcao\n",getpid(),client_pid,ticket_panel);
				sprintf(line,"O cliente %s com senha %d nao se dirigiu ao balcao",client_pid,ticket_panel);
				write_log(line);

				// Actualiza o painel
				/*sem_wait(sem_id);
				tickets_panel_address = (int *) shmat(tickets_panel,0,SHM_RND);
				(*tickets_panel_address)++;
				ticket_panel = *tickets_panel_address;
				*(tickets_panel_address + 1) = getppid();
				shmdt(tickets_panel_address);
				sem_signal(sem_id);*/

				sem_wait(sem_id);
				file_work = fopen("work.log","a+");
				write_log_aux(file_work,line);
				fclose(file_work);
				sem_signal(sem_id);

				sleep(1);
			}
			else // Executa as operacoes pedidas pelo cliente
			{
				printf("CAIXA %d - A espera de pedidos do cliente %s\n",getpid(),client_pid);
				sprintf(line,"A espera de pedidos do cliente %s",client_pid);
				write_log(line);

				sem_wait(sem_id);
				file_work = fopen("work.log","a+");
				write_log_aux(file_work,line);
				fclose(file_work);
				sem_signal(sem_id);

				fifo_client_name = (char *) malloc((sizeof(char) * 11) + sizeof(pid_t));
				sprintf(fifo_client_name,"FIFOcliente%s",client_pid);

				do
				{
					fd = open(fifo_name,O_RDONLY);
					usleep(100000);
					read_line(fd,operation);
					fflush(stdin);
					close(fd);

					if(strcmp(operation,"END") != 0)
					{
						sprintf(line,"Operacao %s pedida pelo cliente %s",operation,client_pid);
						write_log(line);

						sem_wait(sem_id);
						file_work = fopen("work.log","a+");
						write_log_aux(file_work,line);
						fclose(file_work);
						sem_signal(sem_id);

						printf("CAIXA %d - A executar a operacao %s...\n",getpid(),operation);
						rand_num = (random() % 20) + 1;
						num = ((float) rand_num) / 100;
						signal = (random() % 2);

						if(signal)
							operation_wait_time = (operation_base_time - (operation_base_time * num)) * 1000000;
						else
							operation_wait_time = (operation_base_time + (operation_base_time * num)) * 1000000;

						usleep(operation_wait_time);

						printf("Operacao executada\n");
						sprintf(line,"Operacao %s executada em %d ms para o cliente %s",operation,operation_wait_time,client_pid);
						write_log(line);

						printf("A escrever no work.log...\n");

						sem_wait(sem_id);
						file_work = fopen("work.log","a+");
						write_log_aux(file_work,line);
						fclose(file_work);
						sem_signal(sem_id);

						printf("Escreveu no work.log\n");
					}

					rand_num = random() % 3;
					fd = open(fifo_client_name,O_WRONLY);
					write(fd,client_messages[rand_num],strlen(client_messages[rand_num]) + 1);
					close(fd);
				} while(strcmp(operation,"END") != 0);

				sprintf(line,"Fim do atendimento do cliente %s",client_pid);
				write_log(line);

				sem_wait(sem_id);
				file_work = fopen("work.log","a+");
				write_log_aux(file_work,line);
				fclose(file_work);
				sem_signal(sem_id);

				worked_clients++;
			}
		}
		else // Nao ha clientes para atender
		{
			sprintf(line,"Nao ha clientes para atender");
			write_log(line);

			sem_wait(sem_id);
			file_work = fopen("work.log","a+");
			write_log_aux(file_work,line);
			fclose(file_work);
			sem_signal(sem_id);

			sem_wait(sem_id);
			if(service_end) // Tempo de servico ja acabou, logo a caixa termina a sua execuçao
			{
				sem_signal(sem_id);

				sprintf(line,"Tempo de servico terminado");
				write_log(line);

				sem_wait(sem_id);
				file_work = fopen("work.log","a+");
				write_log_aux(file_work,line);
				fclose(file_work);
				sem_signal(sem_id);

				break;
			}

			// Espera 1 segundo para verificar novamente se ha clientes para atender
			sem_signal(sem_id);
			sleep(1);
		}
	}

	sprintf(line,"Fim do funcionamento");
	write_log(line);

	sem_wait(sem_id);
	file_work = fopen("work.log","a+");
	write_log_aux(file_work,line);
	fclose(file_work);
	sem_signal(sem_id);

	sprintf(line,"Numero de clientes atendidos: %d",worked_clients);
	fwrite(line,strlen(line),1,log_file_fd);

	sem_wait(sem_id);
	file_work = fopen("work.log","a+");
	write_log_aux(file_work,line);
	fclose(file_work);
	sem_signal(sem_id);

	return 0;
}
/**************************/


/**********************************************************************/
/*     Thread responsavel por detectar o fim de serviço do balcao     */
/**********************************************************************/
void *detect_service_end(void *args)
{
	int fd;
	int pid;

	//----------------------------------------------------------------
	// Criacao do semaforo
	//----------------------------------------------------------------
	if((sem_id_thread = sem_create(getppid(),1)) == -1)
	{
		printf("Erro na criacao do semaforo com chave %d\n",getppid());
		exit(2);
	}
	//----------------------------------------------------------------


	//----------------------------------------------------------------
	// Espera que o tempo de serviço termine
	//----------------------------------------------------------------
	pid = *(int *) args;
	fifo_name_thread = (char *) malloc((sizeof(char) * 8) + sizeof(pid_t) + 1);
	sprintf(fifo_name_thread,"caixa%daux",pid);
	printf("CAIXA %d - Vai tentar criar FIFO %s\n",main_pid,fifo_name_thread);
	mkfifo(fifo_name_thread,0777);
	printf("CAIXA %d - FIFO %s criado\n",main_pid,fifo_name_thread);
	fd = open(fifo_name_thread,O_RDONLY);
	printf("CAIXA %d - FIFO %s aberto para leitura\n",main_pid,fifo_name_thread);
	printf("CAIXA %d - Terminou tempo de servico\n",main_pid);
	close(fd);
	sem_wait(sem_id_thread);
	service_end = 1;
	sem_signal(sem_id_thread);
	//----------------------------------------------------------------

}
/**********************************************************************/


/**************************************************/
/*     Escreve para o caixaPID.log uma tarefa     */
/**************************************************/
void write_log(char *str)
{
	char line[200];

	time_t actual_time_;
	struct tm *actual_time;

	// Devolve o tempo actual e coloca na estrutura
	time(&actual_time_);
	actual_time = gmtime(&actual_time_);

	// Escreve no ficheiro a tarefa
	sprintf(line,"%.02d/%.02d/%.02d - %.02d:%.02d:%.02d - %s\n",actual_time->tm_mday,(actual_time->tm_mon + 1),(actual_time->tm_year - 100),actual_time->tm_hour,actual_time->tm_min,actual_time->tm_sec,str);
	fwrite(line,strlen(line),1,log_file_fd);
}
/**************************************************/


/**********************************************/
/*     Escreve para o work.log uma tarefa     */
/**********************************************/
void write_log_aux(FILE *file, char *str)
{
	char line[200];

	time_t actual_time_;
	struct tm *actual_time;

	// Devolve o tempo actual e coloca na estrutura
	time(&actual_time_);
	actual_time = gmtime(&actual_time_);

	// Escreve no ficheiro a tarefa
	sprintf(line,"CAIXA %d - %.02d/%.02d/%.02d - %.02d:%.02d:%.02d - %s\n",getppid(),actual_time->tm_mday,(actual_time->tm_mon + 1),(actual_time->tm_year - 100),actual_time->tm_hour,actual_time->tm_min,actual_time->tm_sec,str);
	fwrite(line,strlen(line),1,file);
}
/**********************************************/


/****************************************************/
/*     Limpa os recursos utilizados no processo     */
/****************************************************/
static void clean(void)
{
	if(getpid() != main_pid)
		exit(0);

	printf("CAIXA %d - A limpar recursos utilizados...\n",getpid());
	sem_remove(sem_id);
	sem_remove(sem_id_thread);
	unlink(fifo_name);
	free(fifo_name);
	unlink(fifo_name_thread);
	free(fifo_name_thread);
	free(fifo_client_name);
	free(log_file_name);
	fclose(log_file_fd);
	printf("CAIXA %d - Recursos utlizados limpos\n",getpid());
}
/****************************************************/
