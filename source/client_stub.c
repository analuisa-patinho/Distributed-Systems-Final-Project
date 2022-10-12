//#include <stdio.h> no inet
//#include <stdlib.h> no inet
//#include <string.h> no inet
#include <signal.h>

#include "inet.h"

#include "data.h"
#include "entry.h"
#include "client_stub.h"
#include "client_stub-private.h"
#include "network_client.h"
#include "message-private.h"



struct rtree_t *rtree;
int i;
char *hostname;
char *port;

void handle_sig(int sig);

//Sinal axu
void handle_sig(int sig) {
    printf("A desligar o cliente...\n");
    rtree_disconnect(rtree);
}

/* Função para estabelecer uma associação entre o cliente e o servidor, 
 * em que address_port é uma string no formato <hostname>:<port>.
 * Retorna NULL em caso de erro.
 */
struct rtree_t *rtree_connect(const char *address_port){
    signal(SIGINT, handle_sig);

    //verifica o address_port
    if(address_port == NULL) return NULL;

    hostname = (char *)malloc(80);
    port = (char *)malloc(80);
    strcpy(hostname, strtok((char *) address_port, ":")); 
    strcpy(port, strtok(NULL, ""));

    rtree = (struct rtree_t *)malloc(sizeof(struct rtree_t));
	if(rtree == NULL){
		printf("Erro no malloc(): rtree_connect()\n");
		return NULL;
	}

    // Crira socket TCP
	if ((rtree->socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		free(hostname);
		free(port);
 		perror("Erro ao criar socket TCP");
		return NULL;
	}

    rtree->server.sin_family = AF_INET; // família de endereços
	rtree->server.sin_port = htons(atoi(port)); // Porta TCP

	if (inet_pton(AF_INET, hostname, &rtree->server.sin_addr) < 1) { // Endereço IP
        printf("Erro ao converter IP\n");
		free(hostname);
		free(port);
        close(rtree->socket);
        return NULL;
    }

	if(network_connect(rtree) == 0){  
		free(hostname);
		free(port);
		return rtree; 
	}
	close(rtree->socket); //ADD ver1
	free(hostname);
	free(port);
	return NULL;
}


/* Termina a associação entre o cliente e o servidor, fechando a 
 * ligação com o servidor e libertando toda a memória local.
 * Retorna 0 se tudo correr bem e -1 em caso de erro.
 */
int rtree_disconnect(struct rtree_t *rtree){
    if (rtree == NULL) return -1;
    int result = network_close(rtree);
    if (result >= 0)
        free(rtree);
    return result;
}

/* Função para adicionar um elemento na árvore.
 * Se a key já existe, vai substituir essa entrada pelos novos dados.
 * Devolve 0 (ok, em adição/substituição) ou -1 (problemas).
 */
int rtree_put(struct rtree_t *rtree, struct entry_t *entry){
	return rtree_put2(rtree, entry, 0);
}

/* Função para adicionar um elemento na árvore.
 * Se a key já existe, vai substituir essa entrada pelos novos dados.
 * modo: 0 = cliente, 1 = primario
 * Devolve 0 (ok, em adição/substituição) ou -1 (problemas).
 */
int rtree_put2(struct rtree_t *rtree, struct entry_t *entry, int modo){
	if (entry == NULL || rtree == NULL) {
		printf("Erro rtree_put. \n");
		return -1;
	}

	//Criacao de estruturas de mensagens recebidas e enviadas
	struct message_t *msg = (struct message_t *)malloc(sizeof(struct message_t));
	if (msg == NULL) return -1;
	
	memset(msg, 0, sizeof(struct message_t));

	msg->opcode = OP_PUT; //MESSAGE_T__OPCODE__OP_PUT
	msg->c_type = CT_ENTRY; //MESSAGE_T__C_TYPE__CT_ENTRY
	msg->data_size = entry->value->datasize;
	msg->key = entry->key;
	msg->data = entry->value->data;
	msg->primary = modo;

	struct message_t *msg_resposta;
	msg_resposta = network_send_receive(rtree, msg);
	free_message(msg);

	if(msg_resposta->opcode == OP_ERROR){
		free_message(msg_resposta);
		return -1;	
	}

	int assigned = msg_resposta->op_num;
	printf("Tarefa com o numero: %d\n", assigned);
	free_message(msg_resposta);
	printf("Entry colocada com sucesso\n");
	return 0;

}

/* Função para obter um elemento da árvore.
 * Em caso de erro, devolve NULL.
 */
struct data_t *rtree_get(struct rtree_t *rtree, char *key){
	if (key == NULL || rtree == NULL) return NULL;

	//Criacao de estruturas de mensagens recebidas e enviadas
	struct message_t *msg = (struct message_t *)malloc(sizeof(struct message_t));
	if (msg == NULL) return NULL;

	memset(msg, 0, sizeof(struct message_t));

	msg->opcode = OP_GET;
	msg->c_type = CT_KEY;

	msg->key = key;

	struct message_t *msg_resposta = network_send_receive(rtree, msg);
	free_message(msg);

	if (msg_resposta == NULL){
        return NULL;
	}

	struct data_t *data = NULL;
	if(msg_resposta->opcode != OP_ERROR){
		data = data_create2(msg_resposta->data_size, msg_resposta->data);
		if (data != NULL)
		{
			msg_resposta->data = NULL;
		}
	}
	else
		printf("\nChave nao existente\n");
	
	
	free_message(msg_resposta);
	return data;
}

/* Função para remover um elemento da árvore. Vai libertar 
 * toda a memoria alocada na respetiva operação rtree_put().
 * Devolve: 0 (ok), -1 (key not found ou problemas).
 */
int rtree_del(struct rtree_t *rtree, char *key) {//sempre que é chamada é do cliente
	return rtree_del2(rtree, key, 0); 
}

/* Função para remover um elemento na árvore.
 * Se a key já existe, vai substituir essa entrada pelos novos dados.
 * modo: 0 = cliente, 1 = primario
 * Devolve 0 (ok, em adição/substituição) ou -1 (problemas).
 */
int rtree_del2(struct rtree_t *rtree, char *key, int modo){
	if (key == NULL || rtree == NULL) return -1;

	//Criacao de estruturas de mensagens recebidas e enviadas
	struct message_t *msg = (struct message_t *)malloc(sizeof(struct message_t));
	if(msg == NULL) return -1;

	memset(msg, 0, sizeof(struct message_t));

	msg->opcode = OP_DEL;
	msg->c_type = CT_KEY;
	msg->key = key;
	msg->primary = modo;

	struct message_t *msg_resposta = network_send_receive(rtree, msg);
	free_message(msg);

	//Se a mensagem nao tem conteudo
	if (msg_resposta == NULL ){
		printf("\nNão existe nenhuma entry com a Key\n");
		return -1;
	}
	else if(msg_resposta->opcode == OP_ERROR ){
		printf("\nNão existe nenhuma entry com a Key\n");
		free_message(msg_resposta);
		return -1;
	}

	//a mensagem e libertada
	int assigned = msg_resposta->op_num;
	printf("Tarefa com o numero: %d\n", assigned);
	
	free_message(msg_resposta);
	printf("Chave eliminada com sucesso\n");
	return 0;
}

/* Devolve o número de elementos contidos na árvore.
 */
int rtree_size(struct rtree_t *rtree){
	if (rtree == NULL) return -1;

	//Criacao de estruturas de mensagens recebidas e enviadas
	struct message_t *msg = (struct message_t *)malloc(sizeof(struct message_t));
	if (msg == NULL) return -1;

	memset(msg, 0, sizeof(struct message_t));
	
	msg->opcode = OP_SIZE;
	msg->c_type = CT_RESULT;

	struct message_t *msg_resposta = network_send_receive(rtree, msg);
	free_message(msg);

	//Se a mensagem nao tem conteudo
	if (msg_resposta == NULL){
		return -1;
	}
    if (msg_resposta->opcode == OP_ERROR) {
		free_message(msg_resposta);
        return -1;
    }

	i = msg_resposta->data_size;
	//a mensagem e libertada
	free_message(msg_resposta);
	return i;

}

/* Função que devolve a altura da árvore.
 */
int rtree_height(struct rtree_t *rtree){
	if (rtree == NULL) return -1;

	//Criacao de estruturas de mensagens recebidas e enviadas
	struct message_t *msg = (struct message_t *)malloc(sizeof(struct message_t));
	if (msg == NULL) return -1;

	memset(msg, 0, sizeof(struct message_t));
	
	msg->opcode = OP_HEIGHT;
	msg->c_type = CT_RESULT;

	struct message_t *msg_resposta = network_send_receive(rtree, msg);
	free_message(msg);

	//Se a mensagem nao tem conteudo
	if (msg_resposta == NULL){
		return -1;
	}
    if (msg_resposta->opcode == OP_ERROR) {
		free_message(msg_resposta);
        return -1;
    }

	i = msg_resposta->data_size;
	msg_resposta->data_size = -1;
	//a mensagem e libertada
	free_message(msg_resposta);
	return i;
}

/* Devolve um array de char* com a cópia de todas as keys da árvore,
 * colocando um último elemento a NULL.
 */
char **rtree_get_keys(struct rtree_t *rtree){
	if (rtree == NULL) {
		printf("Erro em rtree_get_keys: 'rtree' NULL.\n");
        return NULL;
    }

	//Criacao de estruturas de mensagens recebidas e enviadas
	struct message_t *msg = (struct message_t *)malloc(sizeof(struct message_t));
	if (msg == NULL) return NULL;

	memset(msg, 0, sizeof(struct message_t));

	msg->c_type = CT_NONE;
	msg->opcode = OP_GETKEYS;

	struct message_t *msg_resposta = network_send_receive(rtree, msg);
	free_message(msg);

    if (msg_resposta == NULL) return NULL;
	if (msg_resposta->opcode != OP_GETKEYS + 1) {
		free_message(msg_resposta);
		return NULL;
	}

	char **ret = msg_resposta->keys;
	msg_resposta->keys = NULL;
	free_message(msg_resposta);

	return ret;
}

/* Liberta a memória alocada por rtree_get_keys().
 */
void rtree_free_keys(char **keys){
	if(keys == NULL) return;
	
	for(int i = 0; keys[i] != NULL; i++) free(keys[i]);
	
	free(keys);
	//return;
}

/* Verifica se a operação identificadapor op_nfoi executada.*/
int rtree_verify(struct rtree_t *rtree,int op_n){
	if (rtree == NULL) return -1;

	//Criacao de estruturas de mensagens recebidas e enviadas
	struct message_t *msg = (struct message_t *)malloc(sizeof(struct message_t));
	if (msg == NULL) return -1;

	memset(msg, 0, sizeof(struct message_t));

	msg->c_type = CT_RESULT;
	msg->opcode = OP_VERIFY;
	msg->op_num = op_n;

	struct message_t *msg_resposta = network_send_receive(rtree, msg);
	free_message(msg);

	if(msg_resposta == NULL) return -1;
	else if (msg_resposta->opcode == OP_ERROR) {
		free_message(msg_resposta);
		return -1;
	}

	int op_num = msg_resposta->op_num;

	free_message(msg_resposta);

	return op_num;
}
