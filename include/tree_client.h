#include "client_stub.h"
#include "network_client.h"
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "inet.h"


//comandos:
// put <key> <data> - OP_PUT CT_ENTRY <entry> devolve: OP_PUT+1 CT_NONE <none> OU OP_ERROR CT_NONE <none> (erro)
// get <key> - OP_GET CT_KEY <key> devolve: OP_GET+1 CT_VALUE <value> OU OP_ERROR CT_NONE <none>
// del <key> - OP_DEL CT_KEY <key> devolve: OP_DEL+1 CT_NONE <none> OU OP_ERROR CT_NONE <none>
// size - OP_SIZE CT_NONE <none> devolve: OP_SIZE+1 CT_RESULT <result>
// height - OP_HEIGHT CT_NONE <none> devolve: OP_HEIGHT+1 CT_RESULT <result>
// getkeys - OP_GETKEYS CT_NONE <none> devolve: OP_GETKEYS+1 CT_KEYS <keys>
// quit


int main(int argc, char **argv);













