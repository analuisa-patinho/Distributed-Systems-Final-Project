#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "entry.h"

/* Função que cria uma entry, reservando a memória necessária e
 * inicializando-a com a string e o bloco de dados passados.
 */
struct entry_t *entry_create(char *key, struct data_t *data){
	if(data == NULL || key == NULL) return NULL; //Null se nao existem dados
    if (strlen(key) <= 0) return NULL; //NULL se nao existe uma key

	struct entry_t *newEntry = (struct entry_t *) malloc(sizeof(struct entry_t)); //reservar memoria para a nova struct
    if(newEntry == NULL ) return NULL;
	newEntry->value = data; // atribuir data da nova struct
	newEntry->key = key; // atribuir key da nova struct
    
	if(newEntry->key == NULL || newEntry->value ==NULL){ //se nao existirem dados
		free(newEntry); //liberta-se a estrutura
		return NULL;
	}

	return newEntry;
}

/* Função que inicializa os elementos de uma entry com o
 * valor NULL.
 */
void entry_initialize(struct entry_t *entry){
	if(entry == NULL) return; //se entry vazia

	entry->key = NULL;
	entry->value = NULL;
}

/* Função que elimina uma entry, libertando a memória por ela ocupada
 */
void entry_destroy(struct entry_t *entry){
	if (entry != NULL){ //se a entry tiver dados
		if (entry->value != NULL)
        	data_destroy(entry->value); // destroi se a data que a entry contem
		if (entry->key != NULL) free(entry->key); // liberta-se a key da entry
		free(entry); //a struct e libertada
	}
}

/* Função que duplica uma entry, reservando a memória necessária para a
 * nova estrutura.
 */
struct entry_t *entry_dup(struct entry_t *entry){
	if (entry == NULL) return NULL; //se a entry nao tem nada

    char *new_key = strdup(entry->key);
    if (new_key == NULL) return NULL;
		
	struct data_t *new_value = data_dup(entry->value);
	if (new_value == NULL) {
		free(new_key);
		return NULL;
	}

	return entry_create(new_key, new_value);
}

/* Função que substitui o conteúdo de uma entrada entry_t.
*  Deve assegurar que destroi o conteúdo antigo da mesma.
*/
void entry_replace(struct entry_t *entry, char *new_key, struct data_t *new_value){
	if(entry == NULL) return;
    if(new_key == NULL) return;
	if(new_value == NULL) return;

    free(entry->key);
    data_destroy(entry->value);
    entry->key = new_key;
    entry->value = new_value;
}

/* Função que compara duas entradas e retorna a ordem das mesmas.
*  Ordem das entradas é definida pela ordem das suas chaves.
*  A função devolve 0 se forem iguais, -1 se entry1<entry2, e 1 caso contrário.
*/
int entry_compare(struct entry_t *entry1, struct entry_t *entry2){
	if(entry1 == NULL || entry2 == NULL ) return 0; //se uma delas for vazia...

	int resultado = strcmp(entry1->key , entry2->key);
	if (resultado == 0) return 0;
	if (resultado > 0) return 1;
	return -1;
}
