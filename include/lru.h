#ifndef LRU_H
#define LRU_H

#include <stddef.h>

typedef struct lru_entry {
    char *key;
    size_t size_bytes;
    unsigned char *data;
    struct lru_entry *prev;
    struct lru_entry *next;
} lru_entry_t;

typedef struct {
    lru_entry_t *head;
    lru_entry_t *tail;
    size_t entry_count;
} lru_list_t;

void lru_init(lru_list_t *list);
void lru_attach_front(lru_list_t *list, lru_entry_t *entry);
void lru_detach(lru_list_t *list, lru_entry_t *entry);
void lru_move_to_front(lru_list_t *list, lru_entry_t *entry);
lru_entry_t *lru_remove_tail(lru_list_t *list);

#endif
