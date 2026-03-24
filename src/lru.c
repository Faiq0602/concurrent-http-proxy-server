#include "lru.h"

#include <stddef.h>

void lru_init(lru_list_t *list)
{
    if (list == NULL) {
        return;
    }

    list->head = NULL;
    list->tail = NULL;
    list->entry_count = 0;
}

void lru_attach_front(lru_list_t *list, lru_entry_t *entry)
{
    if (list == NULL || entry == NULL) {
        return;
    }

    entry->prev = NULL;
    entry->next = list->head;
    if (list->head != NULL) {
        list->head->prev = entry;
    } else {
        list->tail = entry;
    }

    list->head = entry;
    ++list->entry_count;
}

void lru_detach(lru_list_t *list, lru_entry_t *entry)
{
    if (list == NULL || entry == NULL) {
        return;
    }

    if (entry->prev != NULL) {
        entry->prev->next = entry->next;
    } else {
        list->head = entry->next;
    }

    if (entry->next != NULL) {
        entry->next->prev = entry->prev;
    } else {
        list->tail = entry->prev;
    }

    entry->prev = NULL;
    entry->next = NULL;
    if (list->entry_count > 0) {
        --list->entry_count;
    }
}

void lru_move_to_front(lru_list_t *list, lru_entry_t *entry)
{
    if (list == NULL || entry == NULL || list->head == entry) {
        return;
    }

    lru_detach(list, entry);
    lru_attach_front(list, entry);
}

lru_entry_t *lru_remove_tail(lru_list_t *list)
{
    lru_entry_t *tail;

    if (list == NULL || list->tail == NULL) {
        return NULL;
    }

    tail = list->tail;
    lru_detach(list, tail);
    return tail;
}
