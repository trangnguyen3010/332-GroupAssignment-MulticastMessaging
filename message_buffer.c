#include <stdio.h>
#include <stdlib.h>

typedef struct node {
    char *message;
    struct node *next;
} node_t;


int add_msg(node_t **head, char *message) {
    node_t *new_node = malloc(sizeof(node_t));
    if (!new_node){
        return -1;
    }

    new_node->message = message;
    new_node->next = *head;

    *head = new_node;
    return 0;
}

char *get_msg(node_t **head) {
    node_t *current, *prev = NULL;
    char *msg = NULL;

    if (*head == NULL){
        return NULL;
    }

    current = *head;
    while (current->next != NULL) {
        prev = current;
        current = current->next;
    }

    msg = current->message;
    free(current);
    
    if (prev)
        prev->next = NULL;
    else
        *head = NULL;

    
    return msg;
}
