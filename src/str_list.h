#ifndef STRING_LIST_H
#define STRING_LIST_H

#include <pthread.h>

struct str_list {
    pthread_mutex_t mutex;
    struct str_list_node *head;
    struct str_list_node *tail;
    size_t nbytes;
    int count;
    
};

struct str_list_node {
    struct str_list_node *next;
    char str[];
};

int str_list_new_node(struct str_list *list, const char *str);
int str_list_iterate(struct str_list *list, void *ctx, int (*callback)(void *ctx, char *str));
int str_list_concat(struct str_list *list, char sep, char *dest, int destsize);
int str_list_clear(struct str_list *list);
int test_str_list();

int str_list_print(struct str_list *list);

#endif // STRING_LIST_H