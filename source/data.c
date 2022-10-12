#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "data.h"


/* Função que cria um novo elemento de dados data_t e reserva a memória
 * necessária, especificada pelo parâmetro size 
 */
struct data_t *data_create(int size){
  if(size <= 0) return NULL;

  struct data_t *element = (struct data_t *)malloc(sizeof(struct data_t));
  if(element == NULL) return NULL;
  element->data = (void *)malloc(sizeof(void *)*size);
  
  if(element->data == NULL)
  {
	  free(element);
	  return NULL;
  }
  element->datasize = size;

  return element;
}

/* Função idêntica à anterior, mas que inicializa os dados de acordo com
 * o parâmetro data.
 */
struct data_t *data_create2(int size, void *data){
  if (size <= 0 || data == NULL) return NULL;

  struct data_t *element = (struct data_t *)malloc(sizeof(struct data_t));
  if (element == NULL) return NULL;

  element->datasize = size;
  element->data = data;

  return element;
} 

/* Função que elimina um bloco de dados, apontado pelo parâmetro data,
 * libertando toda a memória por ele ocupada.
 */
void data_destroy(struct data_t *data){
  if(data != NULL){
    if(data->data!= NULL)
      free(data->data);
    free(data);
  }
}


/* Função que duplica uma estrutura data_t, reservando a memória
 * necessária para a nova estrutura.
 */
struct data_t *data_dup(struct data_t *data) {
  if(data == NULL ) return NULL;
  if(data->datasize <= 0) return NULL;
  if(data->data == NULL) return NULL;
  
  struct data_t *copy = data_create(data->datasize);
  if (copy == NULL) return NULL;

  memcpy(copy->data, data->data, copy->datasize);

  return copy;
}


/* Função que substitui o conteúdo de um elemento de dados data_t.
*  Deve assegurar que destroi o conteúdo antigo do mesmo.
*/
void data_replace(struct data_t *data, int new_size, void *new_data){
  if(new_size <= 0 ) return;
  if(new_data == NULL) return;

  free(data->data);
  data->datasize = new_size;
  data->data = new_data;
}
