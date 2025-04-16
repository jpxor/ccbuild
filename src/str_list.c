#include "str_list.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

int str_list_new_node(struct str_list *list, const char *str) {
    if (list == NULL || str == NULL) {
        return EINVAL;
    }
    pthread_mutex_lock(&list->mutex);
    size_t len = 1+strlen(str);
    // struct str_list_node *node = arena_alloc(&list->arena, sizeof(*node) + len+1);
    struct str_list_node *node = calloc(1, sizeof(*node) + len);
    if (node == NULL) {
        pthread_mutex_unlock(&list->mutex);
        return ENOMEM;
    }
    strncpy(node->str, str, len);
    if (list->head == NULL) {
        list->head = node;
    } else {
        list->tail->next = node;
    }
    list->tail = node;
    list->count++;
    list->nbytes += len;
    pthread_mutex_unlock(&list->mutex);
    return 0;
}

int str_list_iterate(struct str_list *list, void *ctx, int (*callback)(void *ctx, char *str)) {
    pthread_mutex_lock(&list->mutex);
    struct str_list_node *current = list->head;
    while (current != NULL) {
        int err = callback(ctx, current->str);
        if (err) {
            pthread_mutex_unlock(&list->mutex);
            return err;
        }
        current = current->next;
    }
    pthread_mutex_unlock(&list->mutex);
    return 0;
}


int str_list_print(struct str_list *list) {
    pthread_mutex_lock(&list->mutex);
    struct str_list_node *current = list->head;
    while (current != NULL) {
        printf("strlist: %s\n", current->str);
        current = current->next;
    }
    pthread_mutex_unlock(&list->mutex);
    return 0;
}

int str_list_concat(struct str_list *list, char sep, char *dest, int destsize) {
    assert(list != NULL);

    pthread_mutex_lock(&list->mutex);
    if (dest == NULL) {
        int result = list->nbytes;
        pthread_mutex_unlock(&list->mutex);
        return result;
    }
    assert(destsize > 0);
    char *endp = dest+destsize;

    struct str_list_node *itr = list->head;
    while (itr != NULL) {
        int n = strlen(itr->str);
        if (dest+n >= endp) {
            break;
        }
        strncpy(dest, itr->str, n);
        dest += n;
        dest[0] = sep;
        dest += 1;
        itr = itr->next;
    }
    dest[-1] = 0; // null terminate
    int result = list->nbytes;
    pthread_mutex_unlock(&list->mutex);
    return result;
}

int str_list_clear(struct str_list *list) {
    if (list == NULL) {
        return EINVAL;
    }
    pthread_mutex_lock(&list->mutex);
    list->head = list->tail = NULL;
    // arena_free(&list->arena);
    list->count = 0;
    pthread_mutex_unlock(&list->mutex);
    return 0;
}

int test_str_list() {
    int status = 0;
    struct str_list list = {0};

    // add 3 strings
    str_list_new_node(&list, "one");
    str_list_new_node(&list, "two");
    str_list_new_node(&list, "three");

    if (list.count != 3) {
        printf("test_str_list: wrong count (have %d)\n", list.count);
        status = -1;
    }
    if (strcmp("one", list.head->str) != 0) {
        printf("test_str_list: wrong string (have '%s')\n", list.head->str);
        status = -1;
    }
    if (strcmp("three", list.tail->str) != 0) {
        printf("test_str_list: wrong string (have '%s')\n", list.tail->str);
        status = -1;
    }
    if (list.head->str[3] != 0) {
        printf("test_str_list: strings not null terminated\n");
        status = -1;
    }

    // clear all strings
    str_list_clear(&list);

    if (list.count != 0 || list.head != NULL || list.tail != NULL) {
        printf("test_str_list: failed to clear list\n");
        status = -1;
    }

    // check invalid inputs
    if (str_list_new_node(&list, "") == EINVAL) {
        printf("test_str_list: empty string must not be invalid\n");
        status = -1;
    }
    if (str_list_new_node(&list, NULL) != EINVAL) {
        printf("test_str_list: NULL string must be invalid\n");
        status = -1;
    }
    if (str_list_new_node(NULL, "nullist") != EINVAL) {
        printf("test_str_list: NULL list must be invalid\n");
        status = -1;
    }

    str_list_clear(&list);
    return status;
}