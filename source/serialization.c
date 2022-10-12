#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <netinet/in.h>

#include "tree-private.h"
#include "tree.h"
#include "serialization.h"

/* Serializa uma estrutura data num buffer que será alocado
 * dentro da função. Além disso, retorna o tamanho do buffer
 * alocado ou -1 em caso de erro.
 */
int data_to_buffer(struct data_t *data, char **data_buf){
    if (data == NULL || data_buf == NULL) return -1;
    if (data->data == NULL) return -1;

    uint32_t mem = sizeof(uint32_t) + data->datasize;
    *data_buf = (char *)malloc(mem);
    if (*data_buf == NULL) return -1;

    uint32_t datasize = htonl(data->datasize);
    uint32_t offset = sizeof(uint32_t);
    memcpy(*data_buf, &datasize, sizeof(uint32_t));
    memcpy(*data_buf + offset, data->data, data->datasize);

    return mem;
}

/* De-serializa a mensagem contida em data_buf, com tamanho
 * data_buf_size, colocando-a e retornando-a numa struct
 * data_t, cujo espaco em memoria deve ser reservado.
 * Devolve NULL em caso de erro.
 */
struct data_t *buffer_to_data(char *data_buf, int data_buf_size){
    if (data_buf == NULL || data_buf_size <= 0) return NULL;

    uint32_t datasize = sizeof(uint32_t);
    memcpy(&datasize, data_buf, sizeof(uint32_t));
    datasize = ntohl(datasize);
    if (datasize + sizeof(uint32_t) != data_buf_size) {
        printf("datasize + sizeof(uint32_t) != data_buf_size\n");
        return NULL;
    }

    uint32_t offset = sizeof(uint32_t);

    void *data = malloc(datasize);
    if(data == NULL) return NULL;
    memcpy(data, offset + data_buf , datasize);
    struct data_t *new_Data = data_create2(datasize,data);
    if(new_Data == NULL){
        free(data);
        return NULL;
    }
    return new_Data;
}

/* Serializa uma estrutura entry num buffer que será alocado
 * dentro da função. Além disso, retorna o tamanho deste
 * buffer ou -1 em caso de erro.
 */
int entry_to_buffer(struct entry_t *entry, char **entry_buf){
    if (entry == NULL || entry_buf == NULL) return -1;

    uint32_t keylength = strlen(entry->key) + 1;
    if (keylength <= 0) {
        return -1;
    }
    char *d = NULL;
    uint32_t datasize = data_to_buffer(entry->value, &d);
    if (datasize <= 0) return -1;

    uint32_t mem = sizeof(uint32_t) + keylength + datasize;
    *entry_buf = (char *)malloc(mem);
    if (*entry_buf == NULL) return -1;

    uint32_t keysize = htonl(keylength);
    memcpy(*entry_buf, &keysize, sizeof(uint32_t));
    uint32_t offset = sizeof(uint32_t);

    memcpy(*entry_buf + offset, entry->key, keylength);
    offset += keylength;

    char *tempdatabuf;
    data_to_buffer(entry->value, &tempdatabuf);
    memcpy(*entry_buf + offset, d, datasize);
    
    free(tempdatabuf);
    free(d);

    return mem;
}

/* De-serializa a mensagem contida em entry_buf, com tamanho
 * entry_buf_size, colocando-a e retornando-a numa struct
 * entry_t, cujo espaco em memoria deve ser reservado.
 * Devolve NULL em caso de erro.
 */
struct entry_t *buffer_to_entry(char *entry_buf, int entry_buf_size){
    if (entry_buf == NULL || entry_buf_size <= 0) return NULL;
 
    uint32_t keysize, offset = sizeof(uint32_t);
    memcpy(&keysize, entry_buf, sizeof(uint32_t));
    keysize = ntohl(keysize);

    char *key;
    key = (char *)malloc(keysize);
    if(key == NULL) return NULL;

    memcpy(key, entry_buf + offset, keysize);
    offset += keysize;

    uint32_t datasize = sizeof(uint32_t);
    memcpy(&datasize, entry_buf + offset, sizeof(uint32_t));
    datasize = ntohl(datasize);
    offset += sizeof(uint32_t);

    void *data = malloc(datasize);
    if(data == NULL){
        free(key);
        return NULL;
    }

    memcpy(data, entry_buf + offset, datasize);
    struct data_t *datastruct = data_create2(datasize, data); 
    if(datastruct == NULL) return NULL;
    return entry_create(key, datastruct);
}

/* Serializa uma estrutura tree num buffer que será alocado
 * dentro da função. Além disso, retorna o tamanho deste
 * buffer ou -1 em caso de erro.
 */
int tree_to_buffer(struct tree_t *tree, char **tree_buf){ 
	if (tree == NULL || tree_buf == NULL) return -1; 

    uint32_t size = sizeof(uint32_t);

    struct data_t **value = malloc(tree->size*sizeof(struct data_t)); 
    char **keys = tree_get_keys(tree);
    for(int i = 0; i < tree->size; i++){
        size += sizeof(uint32_t);
        value[i] = tree_get(tree, keys[i]);
        size += value[i]->datasize;
    }

    char *buf = malloc(size);
    int offset = 0;
    memcpy(buf, &size, sizeof(size));
    offset += sizeof(size);

    for(int i = 0; i < tree->size; i++){
        memcpy(buf + offset, &value[i]->datasize, sizeof(value[i]->datasize));
        offset += sizeof(value[i]->datasize);
        char *pointer = buf;
        pointer += offset;
        struct entry_t *entry = entry_create(keys[i], value[i]);
        offset += entry_to_buffer(entry,&pointer);   
    }
    
	return offset;
}

/* De-serializa a mensagem contida em tree_buf, com tamanho
 * tree_buf_size, colocando-a e retornando-a numa struct
 * tree_t, cujo espaco em memoria deve ser reservado.
 * Devolve NULL em caso de erro.
 */
struct tree_t *buffer_to_tree(char *tree_buf, int tree_buf_size){
    if(tree_buf == NULL || tree_buf_size <= 0 ) return NULL;

    struct tree_t *tree = tree_create();
    uint32_t size = 0;
    int offset = 0;

    memcpy(&size, tree_buf, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    for(int i = 0; i < size ; i++){
        int entrySize = 0;
        memcpy(&entrySize, tree_buf + offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        struct entry_t *entry = buffer_to_entry(tree_buf + offset, entrySize);
        tree_put(tree, entry->key, entry->value);
    }
	return tree; 
}
