#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "message-private.h"
#include "sdmessage.pb-c.h"

void free_message(struct message_t *msg){
	if (msg == NULL) return;

	if(msg->key != NULL){
		free(msg->key);
		msg->key = NULL;
	} 
	if(msg->data != NULL){
		free(msg->data);
		msg->data = NULL;
	}
	if(msg->keys != NULL){
		free_keys(msg->keys);
		msg->keys = NULL;
	}  

	free(msg);
}

void free_keys(char **keys){
    int i=0;
	while(keys[i] != NULL){ // percorrer array das keys
        free(keys[i]); // libertar elemento do array
        i++;
    }
    free(keys); // libertar array
}

//Traduz MessageT em message_t
struct message_t* toNotCamel(MessageT * messageT){
	struct message_t * msg = malloc(sizeof(struct message_t));
	memset(msg,0,sizeof(struct message_t));
	msg->opcode = messageT->opcode;
	msg->c_type = messageT->c_type;
	msg->data_size = messageT->data_size;
	//Fase 3
	msg->op_num = messageT->op_num;
	//Fase3

	//Fase4
	msg->primary = messageT->primary;

	if (messageT->key != NULL)
		msg->key = strdup(messageT->key);

	if (messageT->data != NULL){
		msg->data = malloc(messageT->data_size);
		memcpy(msg->data,messageT->data,messageT->data_size);
	}

	if (messageT->keys != NULL){
		msg->keys = calloc(messageT->n_keys + 1, sizeof(char*)); 

		int i;
		for(i = 0; i < messageT->n_keys; i++){
			msg->keys[i] = strdup(messageT->keys[i]);
		}
		msg->keys[i] = NULL;
	}

		/*int i;
		for(i = 0; messageT->keys[i] != NULL; i++){
		}
		msg->keys = malloc((i * (sizeof(char**) + 1));
		if (msg->keys != NULL)
		{
			for(i = 0; messageT->keys[i] != NULL; i++){
				msg->keys[i] = strdup(messageT->keys[i]);
			}
			msg->keys[i] = NULL;
		}
	}*/

	return msg;
}
