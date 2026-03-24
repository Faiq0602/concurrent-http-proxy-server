#include "lru.h"

#include <stdio.h>
#include <string.h>

static int assert_true(int condition, const char *message)
{
    if (!condition) {
        (void)fprintf(stderr, "test failure: %s\n", message);
        return 1;
    }

    return 0;
}

int main(void)
{
    lru_list_t list;
    lru_entry_t first;
    lru_entry_t second;
    lru_entry_t third;
    lru_entry_t *tail;

    (void)memset(&first, 0, sizeof(first));
    (void)memset(&second, 0, sizeof(second));
    (void)memset(&third, 0, sizeof(third));

    lru_init(&list);
    lru_attach_front(&list, &first);
    lru_attach_front(&list, &second);
    lru_attach_front(&list, &third);

    if (assert_true(list.head == &third, "head should be newest") != 0 ||
        assert_true(list.tail == &first, "tail should be oldest") != 0 ||
        assert_true(list.entry_count == 3, "entry count should be tracked") != 0) {
        return 1;
    }

    lru_move_to_front(&list, &first);
    if (assert_true(list.head == &first, "move-to-front should update head") != 0 ||
        assert_true(list.tail == &second, "move-to-front should update tail") != 0) {
        return 1;
    }

    tail = lru_remove_tail(&list);
    if (assert_true(tail == &second, "remove-tail should return least-recent entry") != 0 ||
        assert_true(list.entry_count == 2, "remove-tail should decrement count") != 0) {
        return 1;
    }

    (void)printf("test_lru: all tests passed\n");
    return 0;
}
