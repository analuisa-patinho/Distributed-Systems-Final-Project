#ifndef _TREE_SKEL_H
#define _TREE_SKEL_H

#include "sdmessage.pb-c.h"
#include "message-private.h"
#include "tree.h"

/* Inicia o skeleton da árvore.
 * O main() do servidor deve chamar esta função antes de poder usar a
 * função invoke(). 
 * Retorna 0 (OK) ou -1 (erro, por exemplo OUT OF MEMORY)
 */
int tree_skel_init();

//FAZER DOC
int tree_skel_zooinit(const char *ip, const char *zookeeper);

/* Liberta toda a memória e recursos alocados pela função tree_skel_init.
 */
void tree_skel_destroy();

/* Executa uma operação na árvore (indicada pelo opcode contido em msg)
 * e utiliza a mesma estrutura message_t para devolver o resultado.
 * Retorna 0 (OK) ou -1 (erro, por exemplo, árvore nao incializada)
*/
int invoke(struct message_t *msg);

/* Verifica se a operação identificada por op_nfoi executada.*/
int verify(int op_n);


/* Função do threadsecundário que vai processar pedidos de escrita.*/
void *process_task (void *params);
#endif
