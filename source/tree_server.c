#include "inet.h"
#include "network_server.h"
#include "tree_skel.h"
#include <signal.h>

/*
* Programa servidor: Recebe uma string do cliente e envia o
* tamanho dessa string.
* Como compilar: gcc -Wall -g server.c -o server
* Como executar: ./tree-server <porto_servidor> */
int main(int argc, char **argv) {

	signal(SIGPIPE, SIG_IGN); //sigpipe quando a ligacao quebra

    if (argc != 3){
        printf("Uso: %s <porto_servidor> <zookeeper_host>:<zookeeper_port>\n", argv[0]);
        printf("  <porto_servidor>    - porto TCP.\n");
        printf("  <zookeeper_host>    - ZooKeeper IP ou URL.\n");
        printf("  <zookeeper_port>    - ZooKeeper TCP porto.\n");
        printf("Exemplo de uso: %s 1234 127.0.0.1:2181\n", argv[0]);
        return -1;
    }

    short port = atoi(argv[1]);
    char *zookeeper = argv[2];
	char ip[50];
	strcpy(ip, "127.0.0.1:");
	strcat(ip, argv[1]);

	//sprintf(ip, "127.0.0.1:%d", port);

	if (tree_skel_init() == -1){
        printf("Erro ao inicializar tree_skel_init.\n");
        return -1;
	}

	if (tree_skel_zooinit(ip, zookeeper) == -1){
        printf("Temos os servidores primary e backup a correrem...\n");
        return -1;
	}
	
    int listening_socket = network_server_init(port);

    if(listening_socket  == -1){
		printf("Erro ao inicializar Socket do servidor\n");
		return -1;
	}


	
	printf("Servidor inicializado com sucesso, à espera de clientes.\n");
	printf("Para fechar o servidor: CTRL+C.\n");

	int result = network_main_loop(listening_socket);
	
	tree_skel_destroy();

	network_server_close();

	return result;
}
