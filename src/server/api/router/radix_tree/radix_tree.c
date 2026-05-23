#include "radix_tree.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

struct Node {
    char *name;
    void *payload;

    size_t num_children;
    struct Node **children;
};


node_t *create_node(char* name, void* payload);
void destroy_node(node_t* node);
node_t *find_child(node_t *parent, char* name);
int add_child(node_t *parent, node_t *child);



radix_tree_t *init_tree(char delimeter, char wildcard) {
    radix_tree_t *tree = (radix_tree_t*) malloc(sizeof(radix_tree_t));
    if (!tree) {
        return NULL;
    }

    tree->delimeter = delimeter;
    tree->wildcard  = wildcard;

    node_t *root = create_node("", NULL);
    if (!root) {
        free(tree);
        return NULL;
    }

    tree->root = root;
    
    return tree;
}

void destroy_tree(radix_tree_t *tree) {
    destroy_node(tree->root);
}

int add_node(radix_tree_t *tree, char *name, void *payload) {
    int rc;

    char delim[2] = {tree->delimeter, '\0'};
    char *name_ = strdup(name);

    char* prefix = strtok(name_, delim);
    node_t *parent = tree->root;
    node_t *child;

    while (prefix) {
        child = find_child(parent, prefix);
        if (!child) {
            child = create_node(prefix, NULL);
            if (!child) {
                free(name_);
                return -1;
            }

            rc = add_child(parent, child);
            if (rc < 0) {
                free(name_);
                return -1;
            }
        }
        parent = child;

        prefix = strtok(NULL, delim); 
    }
    parent->payload = payload;

    free(name_);
    return 0;
}

void destroy_node(node_t* node) {
    node_t *child;
    for (size_t i = 0; i < node->num_children; i++) {
        child = node->children[i];
        destroy_node(child);
    }
    free(node->children);
    free(node->name);
    free(node);
}

void *search_node(radix_tree_t *tree, char *name) {
    char delim[2] = {tree->delimeter, '\0'};
    char *name_ = strdup(name);

    char *prefix = strtok(name_, delim);
    node_t *parent = tree->root;
    node_t *child;

    while (prefix) {
        child = find_child(parent, prefix);
        if (!child && tree->wildcard) {
            for (size_t i = 0; i < parent->num_children; i++) {
                if (parent->children[i]->name[0] == tree->wildcard) {
                    child = parent->children[i];
                    break;
                }
            }
        }
        if (!child) {
            free(name_);
            return NULL;
        }
        parent = child;

        prefix = strtok(NULL, delim);
    }

    free(name_);
    return parent->payload;
}


node_t *create_node(char* name, void* payload) {
    node_t *node = (node_t*) malloc(sizeof(node_t));
    if (!node) {
        return NULL;
    }

    node->name = strdup(name);
    node->payload = payload;
    node->num_children = 0;
    node->children = NULL;

    return node;
}

node_t *find_child(node_t *parent, char* name) {
    for (size_t i = 0; i < parent->num_children; i++) {
        if (strcmp(parent->children[i]->name, name) != 0) {
            continue;
        }
        return parent->children[i];
    }
    return NULL;
}


int add_child(node_t *parent, node_t *child) {
    node_t** new_children = (node_t**) realloc(parent->children, (parent->num_children + 1) * sizeof(node_t *));
    if (!new_children) {
        return -1;
    }

    parent->children = new_children;
    parent->children[parent->num_children] = child;
    parent->num_children++;

    return 0;
}