#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "network_server.h"

#define NFDESC 100 // Número de sockets (uma para listening)
#define MAX_MSG 2048
#define TIMEOUT 1000 //timeout de 1000 ms

int lis_socket = 0;
int shutdown_server = 0;

void signal_handler(int signum) {
	
    switch (signum) {
		case SIGINT:
			printf("\nCTRL+C, desligando o servidor...\n");
			shutdown_server = 1;
			break;
		case SIGPIPE:
			printf("\nà espera por clientes...\n");
			break;
		default:
			break;
    }
}

void print_message(struct message_t *msg) {
	printf("\n----- MESSAGE -----\n");
	printf("opcode: %d, c_type: %d\n", msg->opcode, msg->c_type);

	//FASE 3
	if(msg->opcode == OP_VERIFY + 1){
		printf("Result: %d",msg->op_num);
		printf("\n-------------------\n");
		return;
	}
	//FASE 3

	switch(msg->c_type) {
		case CT_ENTRY:{
			printf("Key: %s\n", msg->key);
			printf("Datasize: %d\n", msg->data_size);
		}break;
		case CT_KEY:{
			printf("Key: %s\n", msg->key);
		}break;
		case CT_KEYS:{
			printf("Keys:\n");
			if (msg->keys != NULL){
				for(int i = 0; msg->keys[i] != NULL; i++)
					printf("%s\n", msg->keys[i]);				
			}
		}break;
		case CT_VALUE:{
			printf("Datasize: %d\n", msg->data_size);
		}break;
		case CT_RESULT:{
			printf("Result: %d\n", msg->data_size);
		}break;
		case CT_NONE:{
			printf("None.\n");
		};
	}
	printf("-------------------\n");
}


/* Função para preparar uma socket de receção de pedidos de ligação
 * num determinado porto.
 * Retornar descritor do socket (OK) ou -1 (erro).
 */
int network_server_init(short port){
	struct sockaddr_in server;
	int sockfd,option;

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) { //cria socket TCP
		perror("Erro ao criar socket\n");
		return -1;
	}

	if(setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&option,sizeof(int)) < 0){ //reutilizar endereco
		printf("setsockopt failed\n");
		close(sockfd);
		return -1;
	}
	// preencher estrutura
	server.sin_family = AF_INET;
	server.sin_port = htons(port);
	server.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(sockfd, (struct sockaddr *) &server, sizeof(server)) < 0){ //bind do socket TCP
		perror("Erro ao fazer bind\n");
		close(sockfd);
		return -1;
	}

	if (listen(sockfd, 0) < 0){
		perror("Erro ao executar listen\n");
		close(sockfd);
		return -1;
	}

	return sockfd;   
}

/* Esta função deve:
 * - Aceitar uma conexão de um cliente;
 * - Receber uma msg usando a função network_receive;
 * - Entregar a msg de-serializada ao skeleton para ser processada;
 * - Esperar a resposta do skeleton;
 * - Enviar a resposta ao cliente usando a função network_send.
 */
int network_main_loop(int listening_socket){
	struct sockaddr_in new_client_addr;	
	socklen_t new_client_size;

    int pollsize = 1, i = 0, closed_connections = 0, new_client_sd;
    char buffer[32];

	lis_socket = listening_socket;

	// signals specified by POSIX.
    signal(SIGINT, signal_handler); //interactive attention signal. ficar a espera de clientes
    signal(SIGPIPE, signal_handler); //Broken pipe. ctrl+c

    //max 100 1 server vs 99 clients
    struct pollfd *pollfd = malloc(NFDESC * sizeof(struct pollfd));
    if(pollfd==NULL){
        printf("Erro: malloc pollfd.\n");
        return -1;
    }

	//for (i = 0; i < NFDESC; i++)
    //	pollfd[i].fd = -1;    // poll ignora estruturas com fd < 0
    pollfd[0].fd = listening_socket;
    pollfd[0].revents = 0; // Vamos detetar eventos na welcoming socket
    pollfd[0].events = POLLIN; //esperar ligaçoes nesta socket

    //com o uso do poll temos as sockets que estao abertas
    while (poll(pollfd, pollsize, TIMEOUT) >= 0) { 
        if (pollfd[0].revents & POLLIN) { //se listening socket do servidor tiver dados para ler entao: tentamos aceitar clientes
            new_client_sd = -1;
            
            new_client_size = sizeof(new_client_addr); 

            if(pollsize == NFDESC) { //1 server vs 99 clients, n max 100
                printf("Numero maximo de ligações simultaneas permitidas.\n");
            } else if ((new_client_sd = accept(listening_socket, (struct sockaddr *) &new_client_addr, &new_client_size)) == -1) {
                printf("Erro ao aceitar cliente.\n");
            } else {
                printf("Cliente conectado.\n");
                //Adicionar cliente ao nosso array de polls
                pollfd[pollsize].fd = new_client_sd;
                pollfd[pollsize].revents = 0;
                pollfd[pollsize].events = POLLIN|POLLHUP;
                pollsize++;
            }
        }

        for(i = 1; i<pollsize; i++) { //verificar se algum evento ocorreu em todas as sockets dos nossos clientes
            //Verificar se o cliente continua ativo com o uso do read
            if (recv(pollfd[i].fd, buffer, sizeof(buffer), MSG_PEEK | MSG_DONTWAIT) == 0) { //read == 0 entao o cliente foi abaixo
                printf("Socket do cliente %d fechou. recv == 0 \n", i);
                close(pollfd[i].fd);
                closed_connections = 1;
                pollfd[i].fd = -1;
				//nao feixa o servidor, procuramos por mais clientes
                continue;
            }

			//POLLHUP E POLLIN SEPARADOS PARA ENCONTRAR O ERRO
            if(pollfd[i].revents & POLLHUP) { //caso tenha sido registado POLLHUP nesta socket, fechar socket
                printf("Socket do cliente %d fechou. pollfd[i].revents & POLLHUP\n", i);
                close(pollfd[i].fd);
                closed_connections = 1;
                pollfd[i].fd = -1;
				//nao feixa o servidor, procuramos por mais clientes
                continue;
            }
            
            if(pollfd[i].revents & POLLIN) {//caso tenha dados para ler
                //Tenta receber a msg do cliente
                struct message_t *msg = network_receive(pollfd[i].fd);
                if (msg  == NULL) {
                    perror("Erro ao receber msg do cliente.");
                    close(pollfd[i].fd);
                    closed_connections = 1;
                    pollfd[i].fd = -1;
					//free(msg);
                    continue;
                }
				
				print_message(msg); //imprime "pretty"

                //Esperar a resposta do skeleton;
                if (invoke(msg)!=0) {//Tenta processar a msg
                    perror("Erro com a msg ou com a tree: invoke");
                    close(pollfd[i].fd);
                    closed_connections = 1;
                    pollfd[i].fd = -1;

                    free_message(msg);
                    continue;
                }

				print_message(msg); //imprime "pretty"

				//Enviar a resposta ao cliente usando a função network_send.
                if (network_send(pollfd[i].fd, msg) == -1) {
                    perror("Erro ao enviar msg ao cliente: network_send");
                    close(pollfd[i].fd);
                    closed_connections = 1;
                    pollfd[i].fd = -1;
                    free_message(msg);
                    continue;
                }
                free_message(msg);
            }
        }

        //NO caso de os nossos clientes se tenham desconectado entao
		//andar para traz com o array de polls
        if(closed_connections) {
            for(i = 1; i < pollsize; i++) {//verifica todos os polls
                if(pollfd[i].fd == -1) {//se algum tiver socket fechada: entao passamos todos os polls seguintes para tras
                    for(int j = i; j < pollsize; j++) {
                        pollfd[j].fd = pollfd[j+1].fd;
                        pollfd[j].events = pollfd[j+1].events;
                        pollfd[j].revents = pollfd[j+1].revents;
                    }
                    //deincrementa tamanho do nosso array de polls e o nosso contador 
                    pollsize--;
                    i--;
                }
            } closed_connections = 0;
        }
    }
    
    if(shutdown_server){//fechar todas as Sockets no evento de fechar servidos, CTRL+C
        for(i = 1; i < pollsize; i++){
            if(pollfd[i].fd >= 0) 
                close(pollfd[i].fd);
        }
        network_server_close();
        free(pollfd);
    }

	printf("\nServidor Fechado com Sucesso.\n");
    return 0;
}

/* Esta função deve:
 * - Ler os bytes da rede, a partir do client_socket indicado;
 * - De-serializar estes bytes e construir a msg com o pedido,
 *   reservando a memória necessária para a estrutura message_t.
 */
struct message_t *network_receive(int client_socket){
	uint8_t *buf;
	int nbytes;

	if (client_socket < 0) return NULL;

	MessageT *recv_msg = NULL;
	buf = malloc(MAX_MSG);

	if((nbytes = read(client_socket,buf,MAX_MSG)) < 0){
		perror("Erro ao receber dados do cliente");
		close(client_socket);
	}
	if(nbytes == 0){
		free(buf);
		return NULL;
	} 
	recv_msg = message_t__unpack(NULL, nbytes, buf);
	free(buf);

	if (recv_msg == NULL) {
		fprintf(stdout, "error unpacking message\n");
		return NULL;
	}

	struct message_t * message = toNotCamel(recv_msg);

	message_t__free_unpacked(recv_msg, NULL);

	return message;
}

/* Esta função deve:
 * - Serializar a msg de resposta contida em msg;
 * - Libertar a memória ocupada por esta msg;
 * - Enviar a msg serializada, através do client_socket.
 */
int network_send(int client_socket, struct message_t *msg){
	uint8_t *buf;
	int nbytes;

	if (client_socket < 0 || msg == NULL)
		return -1;

	unsigned len;
	MessageT message;
	message_t__init(&message);
	message.opcode = msg->opcode;
	message.c_type = msg->c_type;
	message.data_size = msg->data_size;
	//message.data = msg->data;
	message.op_num = msg->op_num;
	message.primary = msg->primary;
    
    switch (msg->opcode) {
    case OP_GET + 1:
		message.data = msg->data;
		message.key = msg->key;

        break;
    case OP_GETKEYS + 1:
        message.keys = msg->keys;
        message.n_keys = msg->n_keys;
        break;
    default:
        break;
    }
	
	len = message_t__get_packed_size(&message);

	buf = malloc(len);
	if (buf == NULL) {
		printf("malloc error\n");
		return -1;
	}

	message_t__pack(&message, buf);
	if((nbytes = write(client_socket, buf ,len )) != len){
		perror("Erro ao enviar resposta ao cliente");
		close(client_socket);
	}

	// liberta de free (passaram para a MessageT)
    switch (msg->opcode) {
    case OP_GET + 1:
		msg->data = NULL;
		msg->key = NULL;

        break;
    case OP_GETKEYS + 1:
		msg->keys = NULL;
        break;
    default:
        break;
    }
	
	if (buf != NULL) free(buf);

	return len;
}

/* A função network_server_close() liberta os recursos alocados por
 * network_server_init().
 */
int network_server_close(){
	int n = 0;
	
	if(lis_socket != 0){
		n = close(lis_socket);
		lis_socket = 0;
	}
    return n;
}
