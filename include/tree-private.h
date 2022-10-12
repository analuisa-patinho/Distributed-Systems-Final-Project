#ifndef _TREE_PRIVATE_H
#define _TREE_PRIVATE_H

#include "tree.h"
#include "entry.h" //ver se pode ser incluido

//Estrutura Node
struct node_t{ 
  struct entry_t *entry;
  struct node_t *left; 
  struct node_t *right;
};

struct tree_t {
  struct node_t *root;
  int size; //numero de nos
};

//cria um node
struct node_t *node_create();

//remove TODOS os nodes (recursivamente)
void cleanAll(struct node_t *root);

//get auxiliar
struct data_t *tree_get2(struct node_t *node, char *key);

//node delete
int node_delete(struct tree_t *tree,struct node_t *root,struct node_t *parent, int c, char *key);

//funçao auxiliar de tree_del
struct node_t *searchNew(struct node_t **parent, struct node_t *node, char *key);

//funcao auxiliar de tree_del
void relink (struct node_t* parent, struct node_t* node);

//função que devolve a depth/altura árvore
int depth(struct node_t* node);

//funcao auxiliar do tree_put - recebe node(s)
int put_node(struct node_t *root,struct node_t *parent, int c, struct entry_t *entry);

//funcao auxiliar do tree_get - recebe node
struct data_t *tree_get2(struct node_t *node, char *key);

//funcao auxiliar do tree_get_keys
int counterAux(struct node_t *node, char ** keys, int ap);


#endif 