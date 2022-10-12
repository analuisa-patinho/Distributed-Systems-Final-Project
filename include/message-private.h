#ifndef _MESSAGEPRIVATE_H
#define _MESSAGEPRIVATE_H

#include "errno.h"
#include "error.h"
#include "signal.h"

#include "sdmessage.pb-c.h"

//Define os possíveis opcodes da mensagem 
#define OP_SIZE     10
#define OP_DEL      20
#define OP_GET      30
#define OP_PUT      40
#define OP_GETKEYS  50
#define OP_VERIFY	70
#define OP_HEIGHT   60

//opcode para representar retorno de erro da execução da operação
#define OP_ERROR    99

//Define códigos para os possíveis conteúdos da mensagem (c_type) 
#define CT_KEY      10
#define CT_VALUE    20
#define CT_ENTRY    30
#define CT_KEYS     40
#define CT_RESULT   50

//Opcode representativo de inexistencia de content
#define CT_NONE     60


struct message_t{
	short opcode;
	short c_type;
	int data_size;
	char * data;
	char * key;
	char ** keys;
	int n_keys;
	int op_num; //fase 3
	protobuf_c_boolean primary; //fase4
};

struct message_t* toNotCamel(MessageT * message); 

void free_message(struct message_t *msg);

void free_keys(char **keys);

#endif
