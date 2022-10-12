/*
	Programa cliente para manipular tree remota.

	Uso: ./tree-client <ip servidor>:<porta servidor>
	Exemplo de uso: ./tree-client 127.0.0.1:12345
*/

#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//Fase 4
#ifndef THREADED 
#define THREADED 
#endif
#include <zookeeper/zookeeper.h> 
//Fase 4

#include "client_stub.h"

#define MAX_MSG 2048

//Fase 4
#define ZDATALEN 1024 * 1024

typedef struct String_vector zoo_string; 

char *host_port;
char *zoo_path = "/kvstore";
zhandle_t *zh = NULL;
struct rtree_t *rtree = NULL, *rtail = NULL; //rtree -> primary; rtail -> backup

int is_connected = 0;

#define NODE_ROOT "/kvstore"
#define NODE_PRIMARY "/kvstore/primary"
#define NODE_BACKUP "/kvstore/backup"

typedef struct Stat *stat;

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

void child_watcher(zhandle_t *wzh, int type, int state, const char *zpath, void *watcher_ctx) {
	zoo_string* children_list =	(zoo_string *) malloc(sizeof(zoo_string));
	if (state == ZOO_CONNECTED_STATE)	 {
		if (type == ZOO_CHILD_EVENT) {
	 	   /* Get the updated children and reset the watch */ 
 			if (ZOK != zoo_wget_children(zh, zoo_path, child_watcher, NULL, children_list)) {
 				fprintf(stderr, "Error setting watch at %s!\n", zoo_path);
 			}

			is_connected = 0;
			/*
			fprintf(stderr, "\n=== znode listing === [ %s ]", zoo_path); 
			for (int i = 0; i < children_list->count; i++)  {
				fprintf(stderr, "\n(%d): %s", i+1, children_list->data[i]);
			}
			fprintf(stderr, "\n=== done ===\n");
			*/
		 } 
	 }
	 free(children_list);
}

/**
* Data Watcher function for /MyData node
*/
void data_watcher(zhandle_t *wzh, int type, int state, const char *zpath, void *watcher_ctx) {
	char *zoo_data = malloc(ZDATALEN * sizeof(char));
	 int zoo_data_len = ZDATALEN;
	 if (state == ZOO_CONNECTED_STATE)	 {
		 if (type == ZOO_CHANGED_EVENT) {
	 /* Get the updated data and reset the watch */ 
			 zoo_wget(wzh, zoo_path, data_watcher,(void *)watcher_ctx, zoo_data, &zoo_data_len, NULL);
			 fprintf(stderr, "!!! Data Change Detected !!!\n");
			 fprintf(stderr, "%s\n", zoo_data);
		 } 
	 }
	 free(zoo_data);
}


//retorna 0 se rtree && rtail != NULL
int zooconnect(const char *host_port)
{
	int ret;
	zoo_string *children = NULL;

	if (zh != NULL) //se ja existir -> desaparece
	{
		zookeeper_close(zh);
	}

	if (rtree != NULL) //
	{
		rtree_disconnect(rtree);
		rtree = NULL;
	}

	if (rtail != NULL) //
	{
		rtree_disconnect(rtail);
		rtail = NULL;
	}

	zh = zookeeper_init(host_port, connection_watcher, 2000, 0, 0, 0);

	if (zh == NULL) {
		fprintf(stderr,"Error connecting to ZooKeeper server[%d]!\n", errno);
		exit(EXIT_FAILURE);
	}
	
	ret = zoo_exists(zh, NODE_ROOT, 0, NULL);
	if (ret != ZOK) // ZOK significa que o node existe e a ligacao esta ok
	{
		fprintf(stderr, "Nao consegui validar a existencia %s, devido a %s.\n", NODE_ROOT, zerror(ret)); //zerror funcao do zk que retorna o erro em texto
		zookeeper_close(zh);
		return -1;
	}

	
	children = malloc(sizeof(zoo_string));
	if (children == NULL)
	{
		fprintf(stderr, "Nao consegui alocar memoria.\n");
		zookeeper_close(zh);
		return -1;
	}

	ret = zoo_wget_children(zh, NODE_ROOT, child_watcher, NULL, children); // tem watcher
	if (ret != ZOK)
	{
		fprintf(stderr, "Nao consegui chamar o get_children: %s.\n", zerror(ret));
		zookeeper_close(zh);
		free(children);
		return -1;
	}

	int bufferlen = 160;
	char *buffer = (char *)malloc(160);

	if (buffer == NULL)
	{
		fprintf(stderr, "Nao consegui alocar memoria.\n");
		zookeeper_close(zh);
		free(children);
		return -1;
	}

	for (int i = 0; i < children->count; i++)
	{
		if (strcmp(children->data[i], "primary") == 0)
		{
			ret = zoo_get(zh, NODE_PRIMARY, 0, buffer, &bufferlen, NULL);
			if (ret != ZOK)
			{
				fprintf(stderr, "Nao consegui chamar o get: %s.\n", zerror(ret));
				zookeeper_close(zh);
				free(children);
				return -1;
			}

			if((rtree = rtree_connect(buffer)) == NULL) return -1;

		}
		else if (strcmp(children->data[i], "backup") == 0)
		{
			ret = zoo_get(zh, NODE_BACKUP, 0, buffer, &bufferlen, NULL);
			if (ret != ZOK)
			{
				fprintf(stderr, "Nao consegui chamar o get: %s.\n", zerror(ret));
				zookeeper_close(zh);
				free(children);
				return -1;
			}

			if((rtail = rtree_connect(buffer)) == NULL) return -1;
		}
		free(children->data[i]);
	}
	
	free(children);

	if (rtree == NULL || rtail == NULL)
	{
		fprintf(stderr, "Nao consegui chamar o get: %s.\n", zerror(ret));
		zookeeper_close(zh);
		free(children);
		return -1;
	}

	return 0;
}


int main(int argc, char **argv){
	
	char * s;
	char * token;
	char **keys;

	signal(SIGPIPE, SIG_IGN);

	//Testar os argumentos de entrada
	if(argc != 2){
		printf("Uso: %s <ip servidor>:<porta servidor>\n", argv[0]);
		printf("Exemplo de uso: %s 127.0.0.1:1234\n", argv[0]);
		return -1;
	}

	host_port = argv[1];

	if (zooconnect(host_port) == -1) {
		fprintf(stderr,"Error connecting to ZooKeeper server[%d]!\n", errno);
		exit(EXIT_FAILURE);
	}

/*
    // Iniciar instância do stub e Usar rtree_connect para estabelcer ligação ao servidor
	if((rtree = rtree_connect(argv[1])) == NULL){
		printf("Erro a associar servidor: rtree_connect()\n");
		return -1;
	}*/

	token = "";
	s = (char*) malloc(MAX_MSG);

    //Fazer ciclo até que o utilizador resolva fazer "quit" 
 	while (strcmp(token, "quit") != 0){
		if (is_connected == 0) //se ja existir ligacao -> fechar e abrir nova
		{
			if (zooconnect(host_port) != 0)
				continue;
		}
		
		printf("\n\nEscreva um comando valido:\nput <key> <data>\nget <key>\ndel <key>\nsize\nheight\ngetkeys\nverify\nquit\n");
		printf(">>> "); // Mostrar a prompt para inserção de comando

		// Receber o comando introduzido pelo utilizador
		//   Sugestão: usar fgets de stdio.h
		//   Quando pressionamos enter para finalizar a entrada no
		//   comando fgets, o carater \n é incluido antes do \0.
		//   Convém retirar o \n substituindo-o por \0.
		

		fgets(s, MAX_MSG, stdin);
        s[strlen(s) - 1] = '\0';

		if (strcmp(s, "") == 0){
            printf("Escreva um comando valido:\n\nput <key> <data>\nget <key>\ndel <key>\nsize\nheight\ngetkeys\nverify\nquit\n");
			continue;
		}

		// Verificar se o comando foi "quit". Em caso afirmativo
		//   não há mais nada a fazer a não ser terminar decentemente.
		//    Caso contrário:
		//	Verificar qual o comando;
		//	chamar função do stub para processar o comando e receber msg_resposta
		
		token = strtok(s, " ");

		if(strcmp(token, "quit") == 0)
			break;

        if(strcmp(token, "put") == 0){
		    token = strtok(NULL, " ");
			char *key = strdup(token);
		    token = strtok(NULL, " ");
			//Caso o prompt nao venha com o numero de argumentos correto
			if(token == NULL){
				printf("Escreva um comando valido:\nput <key> <data>\n");
				token = "";
			    free(key);
				continue;
			} else {
				//cria-se uma nova data
				struct data_t *data = data_create2(strlen(token) + 1,token); 
				if (data != NULL) {
					//cria-se uma nova entry
					struct entry_t *entry = entry_create(key,data);
					if(entry != NULL){
						//faz-se put da entry
						int result = rtree_put(rtree, entry);
						if (result == -1)
							printf("Falha ao correr o comando [put] com a chave: %s\n", key);

						entry->key = NULL;
						entry->value = NULL;

						entry_destroy(entry);
					}

					data->data = NULL;
					data_destroy(data);
				}else{
					printf("Falha ao correr o comando [put] com a chave: %s\n", key);
				}
				free(key);

				continue;
			}
		}

        if(strcmp(token, "get") == 0){
	        token = strtok(NULL, " ");
			//copia-se o valor de token para key
	        char *key = strdup(token);

			struct data_t *data = rtree_get(rtail, key);

			if(data != NULL){
                printf("Chave: %s, Tamanho: %d, Dados: %.*s\n", key, data->datasize, data->datasize, (char *)data->data);
                data_destroy(data);
			}else printf("Falha ao correr o comando [get] com a chave: %s\n", key);

			//liberta-se a key
			free(key);
			continue;
		}

		if(strcmp(token, "del") == 0){
	        token = strtok(NULL, " ");
	        char *key = strdup(token);

		    rtree_del(rtree, key);

			free(key);
			continue;
		}

	    if(strcmp(token, "size") == 0){
	        printf("\nNumero de elementos na Arvore: %d\n",rtree_size(rtail));
			continue;
		}

         if(strcmp(token, "height") == 0){
	        printf("\nAltura da Arvore: %d\n",rtree_height(rtail));
			continue;
		}

	    if(strcmp(token, "getkeys") == 0){
	        keys = rtree_get_keys(rtail);
            if (keys != NULL) {

                printf("Chaves:\n");
                int i = 0;
                while (keys[i] != NULL) {
                    printf("\t%d: %s\n", i, keys[i]);
                    i++;
                }

                rtree_free_keys(keys);
				
            } else {
                printf("Falha ao correr o comando [getkeys]\n");
            }
			continue;
		}

		if(strcmp(token, "verify") == 0){
			token = strtok(NULL, " ");
			if(token == NULL){
				printf("Escreva um comando valido:\nverify [OP_N]\n");
				token = "";
				continue;
			}else{
		        char *str = strdup(token);
				int op_n = atoi(str);
				int result = rtree_verify(rtail, op_n);

				if (result == 1)
					printf("\nTerminou a op com o numero: %d\n", op_n);
				else if(result == 0)
					printf("\nEm execucao a op com o numero: %d\n", op_n);
				else
					printf("\nNao terminou a op com o numero: %d\n", op_n);
				free(str);
				continue;
			 }
		}
		printf("Escreva um comando valido:\n\nput <key> <data>\nget <key>\ndel <key>\nsize\nheight\ngetkeys\nverify\nquit\n");
	}
	free(s);
	printf("Desligando o cliente...\n");
  	return rtree_disconnect(rtree);
}
