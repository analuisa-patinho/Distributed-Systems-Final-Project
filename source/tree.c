#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "data.h"
#include "entry.h"
#include "tree-private.h"

////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////NODE/////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

struct node_t *node_create(){
    struct node_t *newNode = ( struct node_t*)malloc(sizeof(struct node_t));
    
    if(newNode == NULL) {
        return NULL;
    }
    
    newNode->entry = NULL;
    newNode->left = NULL;
    newNode->right = NULL;

    return newNode;
}

int put_node(struct node_t *root,struct node_t *parent, int c, struct entry_t *entry){
    if(entry == NULL) return -1;

    if(root == NULL){ 
        struct node_t *newNode = node_create(); //root -> curr ; parent = roo; c=0
        if(newNode == NULL){
            return -1;
        }
        newNode->entry = entry;

        if( c < 0 ){
            parent->right = newNode;
        }else{
            parent->left = newNode; 
        }
    
        return 0;  
    }

    int compare = entry_compare(root->entry, entry);

    if(compare == 0){ 
        entry_replace(root->entry, entry->key, entry->value); 
        return 1;
    } 
    else if (compare < 0){     //recorrente á propria funçao
        return put_node(root->right,root,compare, entry); 
    }
    else { 
        return put_node(root->left,root,compare, entry);
    }
}

struct node_t *searchNew(struct node_t **parent, struct node_t *node, char *key) {  
    if(node == NULL || key == NULL) return NULL;
    int comp = strcmp(node->entry->key, key); //node->key < key = -1
    
    if(comp == 0) return node;
    else if( comp > 0){
        parent = &node;
        return searchNew(parent, node->left, key);
    }
    parent = &node;
    return searchNew(parent, node->right, key); 
}

/*REMOVE UM NODE ESPECIFICO*/ 
int node_delete(struct tree_t *tree, struct node_t *node, struct node_t *parent, int c, char *key){ //se for para o codigo de baixo ha que acrescentar tree

    if(node == NULL) return -1;
    int compare = strcmp(node->entry->key, key); // 1 se key menor que root, -1 se key maior que root
    
    if(compare == 0){ //se eh o nó a apagar
        entry_destroy(node->entry);
        if(node->left == NULL && node->right == NULL){
            if(c < 0){
                parent->right = NULL;
            }else{
                parent->left = NULL;
            }           
        }else if(node->left == NULL){ 
            if(c < 0){
                parent->right = node->right;
            }else{
                parent->left = node->right;
            }        
        }else{ 
            if(c < 0){
                /*parent->right = node->left;*/
                parent->right = node->right;
                //int put_node(struct node_t *root,struct node_t *parent, int c, struct entry_t *entry)
                put_node(parent->right,node->left, c, node->left->entry); 
            }else{
                /*parent->left = node->right;*/
                parent->left = node->left;
                put_node(parent->left, node->right, c, node->right->entry);
            }        
        }
        return 0; 
    }
    else if(compare > 0){
        return node_delete(tree, node->left, node , compare, key);
    }
    else {
        return node_delete(tree, node->right, node, compare, key);
    } 
}

/* Funçao que faz relink dos nos depois de fazer delete
*/
void relink (struct node_t* parent, struct node_t* node){
    if(node->left == NULL &&  node->right == NULL){ //ESTAMOS NO FIM DA TREE
        if(parent != NULL){
            if(parent->left == node) parent->left = NULL; //JA NAO HA
            else if(parent->right == node) parent->right = NULL; //JA NAO HA
        
        free(node); //elimina no
        }
        return;
    }
    else if(node->left != NULL){
        node->entry = node->left->entry; //esta zona de mem passa a ter o valor do left, q pode ser NULL
        relink(node, node->left);
    }else if(node->right != NULL){
        node->entry = node->right->entry;
        relink(node, node->right);       
    }
    
}
////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////TREE/////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

/* Função para criar uma nova árvore tree vazia.
 * Em caso de erro retorna NULL.
 */
struct tree_t *tree_create(){
    struct tree_t *new_tree = ( struct tree_t*)malloc(sizeof(struct tree_t));

    if(new_tree == NULL ){
        return NULL;
    }

    new_tree->size = 0;
    new_tree->root = NULL; 

    return new_tree;
}

 
void cleanAll(struct node_t *root) {
    if(root != NULL){
        cleanAll(root->left);
        root->left = NULL;

        cleanAll(root->right);
        root->right = NULL;

        entry_destroy(root->entry);
        root->entry = NULL;

        free(root);
    }
}


/* Função para libertar toda a memória ocupada por uma árvore.*/
void tree_destroy(struct tree_t *tree){
    if(tree == NULL) return;
    cleanAll(tree->root);
    tree->root = NULL;

    free(tree);
}



/* Função para adicionar um par chave-valor à árvore.
 * Os dados de entrada desta função deverão ser copiados, ou seja, a
 * função vai *COPIAR* a key (string) e os dados para um novo espaço de
 * memória que tem de ser reservado. Se a key já existir na árvore,
 * a função tem de substituir a entrada existente pela nova, fazendo
 * a necessária gestão da memória para armazenar os novos dados.
 * Retorna 0 (ok) ou -1 em caso de erro.
 */
int tree_put(struct tree_t *tree, char *key, struct data_t *value){

    if(key == NULL || value == NULL || tree == NULL) return-1;

    int res;  
    char *newKey = strdup(key); //free da copia mesmo k a root seja null
    if(newKey == NULL) return -1;

    struct data_t *newData = data_dup(value);
    if(newData == NULL){ //verificar se memoria bem alocada
        free(newKey);
        return -1;  
    } 

    struct entry_t *newEntry = entry_create(newKey, newData);

    if(newEntry == NULL){ 
        free(newKey);
        data_destroy(newData);
        return -1;
    }
    if(tree->root == NULL){ 
        struct node_t *newNode = node_create();
        
        if(newNode == NULL){ 
            free(newKey);
            entry_destroy(newEntry);
            return -1;  
        }
        newNode->entry = newEntry; 
        tree->root = newNode;
        tree->size++;

        return 0;
    }else {
        res = put_node(tree->root,tree->root,0, newEntry); 

        if( res == 0 ){ 
            tree->size++;
        }
        if(res == 1) res = 0;
        return res;
    }
}

/* Função para obter da árvore o valor associado à chave key.
 * A função deve devolver uma cópia dos dados que terão de ser
 * libertados no contexto da função que chamou tree_get, ou seja, a
 * função aloca memória para armazenar uma *CÓPIA* dos dados da árvore,
 * retorna o endereço desta memória com a cópia dos dados, assumindo-se
 * que esta memória será depois libertada pelo programa que chamou
 * a função.
 * Devolve NULL em caso de erro.
 */
struct data_t *tree_get(struct tree_t *tree, char *key){ 
    if(tree == NULL || key == NULL) return NULL;
    if(tree->root == NULL) return NULL;
    
    return tree_get2(tree->root, key);
}
struct data_t *tree_get2(struct node_t *node, char *key){
    if(node == NULL) return NULL;
    int compare = strcmp(node->entry->key, key); 
    
    if(compare == 0){
        return data_dup(node->entry->value); 
    } else if(compare < 0){
        return tree_get2(node->right, key); 
    } else {
        return tree_get2(node->left, key);
    }
    return NULL;
}


/* Função para remover um elemento da árvore, indicado pela chave key,
 * libertando toda a memória alocada na respetiva operação tree_put.
 * Retorna 0 (ok) ou -1 (key not found).
 */
 
int tree_del(struct tree_t *tree, char *key){ 
    if(tree == NULL || key == NULL) return -1;
    if(tree->root == NULL) return -1;

    struct node_t *find_parent = tree->root;
    //&findd_parent ele vai escrever em find_parent
    //&find_parent mandamos para dentro da funcao o valor do endereco da var
    //para depois dentro da funcao, escrever o valor da var nesse endereco
    struct node_t *find = searchNew(&find_parent, tree->root, key);
    if(find == NULL) return -1;

    //apagar a entry 
    entry_destroy(find->entry);


    if(find == tree->root){
        if(tree->root->left == NULL && tree->root->right == NULL){ //se nao tem filhos
            free(find);
            tree->root = NULL;
            tree->size = 0;
            return 0;
        }
    }
   
    //religar a arvore
    relink(find_parent, find);

    tree->size--;
    return 0;
}


/* Função que devolve o número de elementos contidos na árvore.*/
int tree_size(struct tree_t *tree){
    if(tree == NULL) return 0;
    return tree->size;
}

/* Função que devolve a depth/altura árvore.*/ 
int depth(struct node_t* node){
   if(node == NULL) return 0;

   int leftD = 0, rightD = 0;
   if(node->left != NULL){
       leftD = depth(node->left) + 1;
   }
   if(node->right != NULL){
       rightD = depth(node->right) + 1;
   }
   if(leftD > rightD){ 
       return leftD;
   }
   return rightD;
}

/* Função que devolve a altura da árvore.*/ 
int tree_height(struct tree_t *tree){
    if (tree->root == NULL)  
       return 0;
    return depth(tree->root);
}


/* Função que devolve um array de char* com a cópia de todas as keys da
 * árvore, colocando o último elemento do array com o valor NULL e
 * reservando toda a memória necessária.
 */
char **tree_get_keys(struct tree_t *tree){ 

    if(tree == NULL) return NULL; 
    char ** keys = malloc((tree->size + 1) * sizeof(char**)); 

	if(keys == NULL) return NULL; //retorna NULL se houver algum erro ao alocar memoria
   
    if(tree->root == NULL) return keys;
    int i = counterAux(tree->root, keys, 0);

    keys[i] =  NULL;
	return keys;
}

/* Funçao aux para o tree_get_keys
*/
int counterAux(struct node_t *node, char ** keys, int ap){
    int n = ap;

    if(node == NULL) return n;

    if(node->entry != NULL){
        if(node->entry->key != NULL){ 
            keys[n] = strdup(node->entry->key);
            n++;
        }
    }
    if(node->left != NULL){
        n = counterAux(node->left, keys, n);
    }
    if(node->right != NULL){
        n = counterAux(node->right, keys, n);
    }
    return n;
}

/* Função que liberta toda a memória alocada por tree_get_keys().
 */
void tree_free_keys(char **keys){
    int i=0;
	while(keys[i] != NULL){ // percorrer array das keys
        free(keys[i]); // libertar elemento do array
        i++;
    }
    free(keys); // libertar array
}
