#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#ifndef THREADED 
#define THREADED 
#endif

#include <zookeeper/zookeeper.h>

#include "tree_skel.h"
#include "client_stub.h"
//#include "zookeeper/zookeeper.h"

#define NODE_PRIMARY "/kvstore/primary"
#define NODE_BACKUP "/kvstore/backup"
#define NODE_ROOT "/kvstore"

typedef struct String_vector zoo_string;

struct tree_t *tree = NULL;//Arvore de entradas do servidor
struct rtree_t *rtreeB = NULL; // ligacao ao nosso servidor de BACKUP
//char * enderecoB = NULL; // endereco do servidor de BACKUP, tal como ele colocou no node
const char *myEndereco = NULL;

//Fase 3
struct task_t {
    int op_n;//o número da operação
    int op;//a operação a executar: op=0 se for um delete, op=1 se for um put
    char *key;//a chave a remover ou adicionar
    struct data_t *data;//os dados a adicionar em caso de put, ou NULL em caso de delete
	//campos necessarios para: produtor/consumidor
    struct task_t *next;// próximo elemento da fila
	int data_size;
};

struct task_t *queue_head = NULL;//cabeça da fila de tarefas por executar
int last_assigned = 0;//Sempre que é recebido um novo pedido de escrita, o servidor responde com o estado atual de last_assigned e de seguida incrementa-o por uma unidade.
int op_count = 0;//Sempre que uma thread executa uma operação, o valor de op_count é incrementado por um.

int thread_run = 0;//Variável condicional para saber quando a thread secundária deve terminar
pthread_t thread_fila;//Thread secundária

pthread_mutex_t queue_lock = PTHREAD_MUTEX_INITIALIZER;//Locks para a queue
pthread_mutex_t tree_lock = PTHREAD_MUTEX_INITIALIZER;//Locks para a tree
pthread_cond_t queue_not_empty = PTHREAD_COND_INITIALIZER;//Var condicional para o lock da queue

int quit;

void add_task(struct message_t *msg);
void task_destroy(struct task_t *task);
//Fase 3 end

// Fase 4

/* ZooKeeper Znode Data Length (1MB, the max supported) */
#define ZDATALEN 1024 * 1024

char *host_port;
char *root_path = "/kvstore";
zhandle_t *zh;
int is_connected;
int somos_primary = 0;
//static char *watcher_ctx = "ZooKeeper Data Watcher";

/**
* Watcher function for connection state change events
*/
void connection_watcher(zhandle_t *zzh, int type, int state, const char *path, void* context) {
	if (type == ZOO_SESSION_EVENT) {
		if (state == ZOO_CONNECTED_STATE) {
			is_connected = 1; 
		} else {
			is_connected = 0; 
		}
	} 
}
/**
* Data Watcher function for /MyData node
* Chamada que sempre ha alteracao no children do nó que estamos a ver
*/
void child_watcher(zhandle_t *wzh, int type, int state, const char *zpath, void *watcher_ctx) {
	int ret = 0;
	zoo_string* children =	(zoo_string *) malloc(sizeof(zoo_string));

	if (state == ZOO_CONNECTED_STATE){
		if (type == ZOO_CHILD_EVENT) {
	 	   /* Get the updated children and reset the watch */ 

			ret = zoo_wget_children(wzh, root_path, child_watcher, watcher_ctx, children);
			
 			if (ZOK != ret) {
 				fprintf(stderr, "Error setting watch at %s!\n", root_path);
 			}
			else if (ret == ZOK){
				int temPrimary = 0;
				int temBackup = 0;

				for (int i = 0; i < children->count; i++){
					if (strcmp(children->data[i], "primary") == 0){
						temPrimary = 1;
					}
					else if (strcmp(children->data[i], "backup") == 0){
						temBackup = 1;
					}
					free(children->data[i]);
				}

				if (children->data != NULL)	free(children->data);

				//ligar ao backup ou auto-promover
				if (somos_primary){
					if (temBackup){
						int bufferlen = 160;
						char *buffer = (char *)malloc(160);

						if (buffer == NULL){
							fprintf(stderr, "Nao consegui alocar memoria.\n");
						}
						else{
							// temos de ter os dados do backup, q vamos usar no nosso rtreeBackup
							ret = zoo_get(wzh, NODE_BACKUP, 0, buffer, &bufferlen, NULL);
							if (ret == ZOK){
								if (rtreeB != NULL) { 
									rtree_disconnect(rtreeB);
									rtreeB = NULL; 
								}
								
								rtreeB = rtree_connect(buffer);  

								if (rtreeB != NULL) {
									printf("Ligado ao BACKUP: %s\n", buffer);
								}
							}
							else{
								fprintf(stderr, "Nao consegui chamar o get: %s.\n", zerror(ret));
							}
							free(buffer);
							
						}
					}
					else{
						// temos de limpar o rtreeBackup, para que possa ser reutilizado
						if (rtreeB != NULL){ 
							rtree_disconnect(rtreeB);
							rtreeB = NULL; 
						}
					}
				}
				else if (!somos_primary && !temPrimary){
					// temos de nos auto-promover para PRIMARY
					// temos de ser primary
					somos_primary = 1;
					ret = zoo_create(wzh, NODE_PRIMARY, myEndereco, strlen(myEndereco) + 1, &ZOO_OPEN_ACL_UNSAFE, ZOO_EPHEMERAL, NULL, 0);
					if (ret == ZOK){
						printf("Servidor PRIMARY com o endereco: %s\n", myEndereco);

						// se conseguirmos, vamos apagar o node BACKUP (o nosso)
						ret = zoo_delete(wzh, NODE_BACKUP, -1);
						// o que se faz se nao conseguirmos apagar o antigo node BACKUP??
						printf("Apagamos o NODE BACKUP: %s\n", zerror(ret));

					}
					else{
						somos_primary = 0;
						fprintf(stderr, "Nao foi criado o %s devido a %s\n", NODE_PRIMARY, zerror(ret));
					}

				}
			}
		 } 
	 }
	 free(children);	
}

/* Inicia o skeleton da árvore.
 * O main() do servidor deve chamar esta função antes de poder usar a
 * função invoke(). 
 * Retorna 0 (OK) ou -1 (erro, por exemplo OUT OF MEMORY)
 */
int tree_skel_init(){
	//criacao da arvore

    tree = tree_create();
    if(tree == NULL) {
		 printf("Erro ao criar tree_skel_init.\n");
		return -1;
	}

    thread_run = 1; //CRIAR THREAD DA FILA DE ESPERA
    if(pthread_create(&thread_fila, NULL, &process_task, NULL) != 0) {
        printf("tree_skel_init: pthread_create()\n");
		tree_destroy(tree); 
        return -1;
    }
    return 0;
}

int tree_skel_zooinit(const char* ip, const char *zookeeper){
	zoo_string *children = NULL;

	//Fase 4 igual ao dado pelos stores
	char node_path[120] = "";
	strcat(node_path,root_path); 
	strcat(node_path,"/primary"); 

	if (ip == NULL || zookeeper == NULL) return -1;

	myEndereco = ip; 

	zh = zookeeper_init(zookeeper, connection_watcher, 2000, 0, 0, 0);
	if (zh == NULL) {
		printf("Erro ao conectar com o ZooKeeper server!\n");
		return -1;
	}

	int ret;

	ret = zoo_exists(zh, root_path, 0, NULL);
	if (ret == ZNONODE){
		ret = zoo_create(zh, root_path, NULL, -1, &ZOO_OPEN_ACL_UNSAFE, 0, NULL, 0);
		if (ret != ZOK){
			fprintf(stderr, "Nao foi criado o %s, devido a %s.\n", root_path, zerror(ret)); //zerror funcao do zk que retorna o erro em texto
			zookeeper_close(zh);
			return -1;
		}
	}
	else if (ret != ZOK) { // ZOK significa que o node existe e a ligacao esta ok
		fprintf(stderr, "Nao consegui validar a existencia %s, devido a %s.\n", root_path, zerror(ret)); //zerror funcao do zk que retorna o erro em texto
		zookeeper_close(zh);
		return -1;
	}

	/*Check Primary
		-- se existir e nao houver backup -> somos Backup
		-- se nao existir -> somos
		--se existir os dois -> deus
	*/

	children = malloc(sizeof(zoo_string));
	if (children == NULL){
		fprintf(stderr, "Nao conseguio alocar memoria. malloc(sizeof(zoo_string).\n");
		zookeeper_close(zh);
		return -1;
	}

	ret = zoo_wget_children(zh, NODE_ROOT, child_watcher, NULL, children); // tem watcher
	if (ret != ZOK){
		fprintf(stderr, "Nao conseguio chamar o get_children: %s.\n", zerror(ret));
		zookeeper_close(zh);
		return -1;
	}

	int temPrimary = 0;
	int temBackup = 0;

	for (int i = 0; i < children->count; i++){
		if (strcmp(children->data[i], "primary") == 0){
			temPrimary = 1;
		}
		else if (strcmp(children->data[i], "backup") == 0){
			temBackup = 1;
		}
		free(children->data[i]);
	}

	if (children->data != NULL) free(children->data);

	free(children);

	if (temPrimary == 1 && temBackup == 1){
		fprintf(stderr, "Primary e Backup up.\n");
		zookeeper_close(zh);
		return -1;
	}
	else if (temPrimary == 1){
		// ip "127.0.0.1:1234"
		// temos de ser backup
		somos_primary = 0;
		ret = zoo_create(zh, NODE_BACKUP, ip, strlen(ip) + 1, &ZOO_OPEN_ACL_UNSAFE, ZOO_EPHEMERAL, NULL, 0);
		if (ret != ZOK){
			fprintf(stderr, "Nao consegui criar o %s devido a %s\n", NODE_BACKUP, zerror(ret));
			zookeeper_close(zh);
			return -1;
		}
		printf("Servidor BACKUP com o endereco %s\n", ip);
	}
	else
	{
		somos_primary = 1;
		// temos de ser primary
		ret = zoo_create(zh, NODE_PRIMARY, ip, strlen(ip) + 1, &ZOO_OPEN_ACL_UNSAFE, ZOO_EPHEMERAL, NULL, 0);
		if (ret != ZOK)
		{
			somos_primary = 0;
			fprintf(stderr, "Nao conseguio criar o %s devido a %s\n", NODE_PRIMARY, zerror(ret));
			zookeeper_close(zh);
			return -1;
		}
		printf("Servidor PRIMARY com o endereco %s\n", ip);
	}

	return 0;
}

/* Liberta toda a memória e recursos alocados pela função tree_skel_init.
 */
void tree_skel_destroy(){
	if(tree != NULL){
		tree_destroy(tree);
		tree = NULL;
	}

	if (thread_run == 1){
		pthread_mutex_lock(&queue_lock);
		quit = 1;
		pthread_cond_signal(&queue_not_empty);
		pthread_mutex_unlock(&queue_lock);

		// Faz 'join' dos processos
		if (pthread_join(thread_fila,NULL) != 0)
			printf("tree_skel_destroy: pthread_join()\n");
	
		thread_run = 0;
	}
}

/* Executa uma operação na árvore (indicada pelo opcode contido em msg)
 * e utiliza a mesma estrutura message_t para devolver o resultado.
 * Retorna 0 (OK) ou -1 (erro, por exemplo, árvore nao incializada)
*/
int invoke(struct message_t *msg){
    if(tree == NULL || msg == NULL) {
		printf("Erro no invoke: tree||msg == NULL.\n");
		return -1;
	}

    struct data_t *data = NULL;

	//Controlo de codigos
    if (!(msg->opcode == OP_SIZE || msg->opcode == OP_DEL || msg->opcode == OP_GET || msg -> opcode == OP_PUT || msg->opcode == OP_GETKEYS || msg->opcode == OP_HEIGHT || msg->opcode == OP_VERIFY))
		return -1;

    switch(msg->opcode){
        case OP_SIZE:

			if (somos_primary)
			{
				// rejeitamos
				msg->opcode = OP_ERROR;
				msg->c_type = CT_NONE;
			}
			else
			{
				pthread_mutex_lock(&tree_lock);
				msg->opcode = OP_SIZE + 1;
				msg->c_type = CT_RESULT;
				msg->data_size = tree_size(tree);
				pthread_mutex_unlock(&tree_lock);
			}

            break;
        //OP_SIZE
		case OP_HEIGHT:
			if (somos_primary)
			{
				// rejeitamos
				msg->opcode = OP_ERROR;
				msg->c_type = CT_NONE;
			}
			else
			{
				pthread_mutex_lock(&tree_lock);
				msg->opcode = OP_HEIGHT + 1;
				msg->c_type = CT_RESULT;
				msg->data_size = tree_height(tree);
				pthread_mutex_unlock(&tree_lock);
			}
			break;
        //OP_HEIGHT
		case OP_DEL:
			//Fazer se somos primary ou se somos backup e msg vem do primary
			if (somos_primary || (!somos_primary && msg->primary)) 
			{
				if (msg->key == NULL) {
					msg->opcode = OP_ERROR;
					printf("Invoke() 1: Key == NULL\n");
					break;
				}

				add_task(msg); //Mutex

				msg->opcode = OP_DEL + 1;
				last_assigned++; // -----> REVER REVER REVER <-----
				msg->op_num = last_assigned;
					
			}
			else
			{
				// rejeitamos
				msg->opcode = OP_ERROR;
				msg->c_type = CT_NONE;
			}
			break;
        //OP_DEL
		case OP_GET:
			if (somos_primary)
			{
				// rejeitamos
				msg->opcode = OP_ERROR;
				msg->c_type = CT_NONE;
			}
			else
			{
				if (msg->key == NULL) {
					msg->opcode = OP_ERROR;
					msg->c_type = CT_NONE;
					printf("Invoke(): OP_GET key NULL\n");
					break;
				}

				pthread_mutex_lock(&tree_lock);
				data = tree_get(tree, msg->key);
				pthread_mutex_unlock(&tree_lock);

				if(data == NULL){
					msg->c_type = CT_NONE;
					msg->opcode = OP_ERROR;
					msg->data_size = 0;
					break;
				}else{
					msg->c_type = CT_VALUE;
					msg->opcode = OP_GET+1;
					msg->data_size = data->datasize;
					msg->data = malloc(data->datasize);
					memcpy(msg->data,data->data,data->datasize);
					last_assigned++;
					msg->op_num = last_assigned; 
					data_destroy(data);
				}
			}
			// Libertação de memória
			break;
        //OP_GET
		case OP_PUT:
			//Fazer se somos primary ou se somos backup e msg vem do primary
			if (somos_primary || (!somos_primary && msg->primary)) 
			{
				if (msg->key == NULL) {
					msg->opcode = OP_ERROR;
					msg->c_type = CT_NONE;
					printf("Invoke(): OP_PUT key NULL\n");
					break;
				}

				if (msg->data == NULL) {
					msg->opcode = OP_ERROR;
					msg->c_type = CT_NONE;
					printf("Invoke(): OP_PUT data NULL\n");
					break;
				}

		
				add_task(msg);//Mutex

				msg->c_type = CT_RESULT;
				msg->opcode = OP_PUT + 1;
				last_assigned++; // -----> REVER REVER REVER <-----
				msg->op_num = last_assigned;

				free(msg->data);
				msg->data = NULL;
	
			}
			else
			{
				// rejeitamos
				msg->opcode = OP_ERROR;
				msg->c_type = CT_NONE;
			}
			
		
			break;
		//OP_PUT
		case OP_GETKEYS:
		if (somos_primary)
			{
				// rejeitamos
				msg->opcode = OP_ERROR;
				msg->c_type = CT_NONE;
			}
			else
			{
				msg->opcode = OP_GETKEYS + 1;
				msg->c_type = CT_KEYS;
				pthread_mutex_lock(&tree_lock);
				int tamanho = tree_size(tree);
				char **n_keys = tree_get_keys(tree);
				pthread_mutex_unlock(&tree_lock);

				msg->keys = n_keys;
				msg->n_keys = tamanho;
			}
			break;
		case OP_VERIFY:
			if (somos_primary)
			{
				// rejeitamos
				msg->opcode = OP_ERROR;
				msg->c_type = CT_NONE;
			}
			else
			{
				pthread_mutex_lock(&queue_lock);
				int verify_OP = verify(msg->op_num);
				pthread_mutex_unlock(&queue_lock);
				if (verify_OP >= 0) {
					msg->opcode = OP_VERIFY + 1;
					msg->c_type = CT_RESULT;
					msg->op_num = verify_OP;
				} else {
					msg->opcode = OP_ERROR;
					msg->c_type = CT_NONE;
				}
			}
			break;
		//OP_VERIFY
		default:
			msg->opcode = OP_ERROR;
			msg->c_type = CT_NONE;
			return -1;
    }
    return 0; 
}


/* Função do threadsecundário que vai processar pedidos de escrita.*/
void *process_task (void *params) {
	//Atenção  que paramspode potencialmente ser NUL
	int res = 0;
	while(1){ 
		//bloqueia fila
		pthread_mutex_lock(&queue_lock);
		while(queue_head == NULL){ //deforma a prevenir o spurious wakeup
			pthread_cond_wait(&queue_not_empty, &queue_lock); /* Espera haver algo */
			if(quit == 1) pthread_exit(0);
		}
		struct task_t *task = queue_head;
		//OP_PUT
		if(task->op == 1){ 
			struct entry_t *entry = entry_create(task->key,task->data);
			if(entry == NULL){
				printf("Erro entry_create: OP_PUT");
			}
			else{
				task->key = NULL; // passaram para a entry
				task->data = NULL;

				pthread_mutex_lock(&tree_lock);
				res = tree_put(tree, entry->key, entry->value); 
				pthread_mutex_unlock(&tree_lock);
				
				if (somos_primary && rtreeB != NULL){ // aceitar e enviar para o BACKUP 
					if (rtree_put2(rtreeB, entry, 1) != 0){
						printf("Erro rtree_put2 em B: OP_PUT");
					}
				}
				// estamos a fazer o destroy do entry??
				entry_destroy(entry); 
			}
		}

		//OP_DEL
		if(task->op == 0){
			pthread_mutex_lock(&tree_lock);
			res = tree_del(tree, task->key);
			pthread_mutex_unlock(&tree_lock);
			if(res == -1)
				printf("Erro: tree_del()\n");

			if (somos_primary && rtreeB != NULL){ // aceitar e enviar para o BACKUP
				if (rtree_del2(rtreeB, task->key, 1) != 0) { //1 <- Modo aka do server
					printf("Erro rtree_del2 em B: OP_DEL");
				}
			}

		}

		queue_head = task->next;

	    op_count++;
        task_destroy(task); 

		pthread_mutex_unlock(&queue_lock);
	}
}


void add_task(struct message_t *msg) {
	pthread_mutex_lock(&queue_lock);

	struct task_t *task = malloc(sizeof(struct task_t));
	if(task == NULL) return;

	task->op_n = last_assigned;

	if(msg->opcode == OP_PUT) {
		task->op = 1;
		task->data_size = msg->data_size;
		void *d = malloc(msg->data_size);
		memcpy(d, msg->data, msg->data_size);
		
		task->data = data_create2(task->data_size, d);
		task->key = strdup(msg->key);
	}

	if(msg->opcode == OP_DEL){
		task->op = 0;
		task->data_size = msg->data_size;
		task->data = NULL;
		task->key = strdup(msg->key);
	}

	if(queue_head==NULL) { /* Adiciona na cabeça da fila */
		queue_head = task; 
		task->next=NULL;
	} else { /* Adiciona no fim da fila */
		struct task_t *tptr = queue_head;
		while(tptr->next != NULL) 
			tptr=tptr->next;
		tptr->next=task; 
		task->next=NULL;
	}

	pthread_cond_signal(&queue_not_empty); /* Avisa um bloqueado nessa condição */
	pthread_mutex_unlock(&queue_lock);
}

// Função privada para destruição de uma tarefa
void task_destroy(struct task_t *task) {
    if (task != NULL) {
        if (task->key != NULL) {
            free(task->key);
        }

        if (task->data != NULL) {
            data_destroy(task->data);
        }

        free(task);
    }
};

/* Verifica se a operação identificada por op_n foi executada.
*/
int verify(int op_n) {
	if (op_n < 0) {
        printf("Erro em verify: 'op_n' menor que zero.\n");
        return -1;
    }
	//if(op_n > last_assigned) return -1;

	return op_n < op_count ? -1 : 1;
}
