#ifndef RADIX_H
#define RADIX_H

#include <stddef.h>

typedef struct Node node_t;

typedef struct radix_tree {
    node_t *root;
    char delimeter;
    char wildcard;
} radix_tree_t;

radix_tree_t *init_tree(char delimeter, char wildcard);
void destroy_tree(radix_tree_t* tree);
int add_node(radix_tree_t *tree, char *name, void *payload);
void *search_node(radix_tree_t *tree, char *name);


#endif /* RADIX_H */