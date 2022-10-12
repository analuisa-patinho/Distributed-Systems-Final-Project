#include "network_client.h"
#include "client_stub-private.h"
#include "message-private.h" //write e read all

#include <errno.h>
#include <inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_MSG 2048
uint8_t *buf = NULL;

/* Esta função deve:
 * - Obter o endereço do servidor (struct sockaddr_in) a base da
 *   informação guardada na estrutura rtree;
 * - Estabelecer a ligação com o servidor;
 * - Guardar toda a informação necessária (e.g., descritor do socket)
 *   na estrutura rtree;
 * - Retornar 0 (OK) ou -1 (erro).
 */
int network_connect(struct rtree_t *rtree){
    //cria socket
    if((rtree->socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Erro ao criar socket TCP");
        return -1;
    }

    //Estabelecer a ligação com o servidor;
    if(connect(rtree->socket,(struct sockaddr *)&rtree->server, sizeof(rtree->server)) < 0 ) {
        perror("Erro ao conectar-se ao servidor");
        close(rtree->socket);
        return -1;
    }

    
    return 0;
}

/* Esta função deve:
 * - Obter o descritor da ligação (socket) da estrutura rtree_t;
 * - Serializar a mensagem contida em msg;
 * - Enviar a mensagem serializada para o servidor;
 * - Esperar a resposta do servidor;
 * - De-serializar a mensagem de resposta;
 * - Retornar a mensagem de-serializada ou NULL em caso de erro.
 */
struct message_t *network_send_receive(struct rtree_t * rtree, struct message_t *msg){
    if(rtree == NULL || msg == NULL) return NULL;
    if(rtree->socket < 0 ) return NULL;

	int len, nbytes;
	MessageT message;
	message_t__init(&message);
	message.c_type = msg->c_type;
	message.opcode = msg->opcode;
	message.op_num = msg->op_num;
	message.data_size = msg->data_size;
	message.data = NULL;
	message.key = NULL;
	message.keys = NULL;
	message.primary = msg->primary;

	if(msg->opcode == OP_DEL){
		message.key = msg->key;
	}

	if(msg->opcode == OP_GET){
		message.key = msg->key;
	}

	if(msg->opcode == OP_PUT){
		message.key = msg->key;
		message.data = msg->data;
	}

	len = message_t__get_packed_size(&message);	
	buf = malloc(len);
	if (buf == NULL) {
		fprintf(stdout, "malloc error\n");
		return NULL;
	}

	msg->key = NULL;
	msg->data = NULL;

	message_t__pack(&message, buf);
	if((nbytes = write(rtree->socket, buf , len)) != len){
        	perror("Erro ao enviar dados ao servidor");
        	close(rtree->socket);
			free(buf);
        	return NULL;
    	}

	printf("\nÀ espera de resposta do servidor ...\n");
	free(buf); //libertar o buf
	buf = malloc(MAX_MSG);
	MessageT *recv_msg = NULL;
	if((nbytes = read(rtree->socket,buf, MAX_MSG)) <= 0) {
		perror("Erro ao receber dados do servidor");
		close(rtree->socket);
		free(buf);
		return NULL;
	}

    //De-serializacao a mensagem de resposta
	recv_msg = message_t__unpack(NULL, nbytes, buf);
	if (buf != NULL) free(buf);

	if (recv_msg == NULL) {
		fprintf(stdout, "error unpacking message\n");
		return NULL;
	}

	struct message_t * result = toNotCamel(recv_msg);
	message_t__free_unpacked(recv_msg, NULL);
	return result;
}

/* A função network_close() fecha a ligação estabelecida por
 * network_connect().
 */
int network_close(struct rtree_t *rtree){
    if(rtree == NULL) return -1;
    return close(rtree->socket); //Fecha socket 
}
