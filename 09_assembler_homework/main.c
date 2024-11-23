#include <stdio.h>
#include <stdlib.h>

char empty_str = 0;
const char* int_format = "%ld ";

long data[] = {4, 8, 15, 16, 23, 42};
size_t data_length = sizeof(data) / sizeof(data[0]);

void print_int(long value) {
    printf(int_format, value);
    fflush(stdout);
}

long p(long value) { // функция predicate
    return value & 1;
}

typedef struct List {
    long value;
    struct List *next;
} List;

List* add_element(long value, List **list) { // функция push
    List* elem = malloc(sizeof(List));
    if (elem == NULL) {
        fprintf(stderr, "Невозможно выделить память для нового элемента со значением %ld\n", value);
        return *list;
        //abort();
    }
    elem->value = value;
    elem->next = (*list);
    return elem;
}

void m(List **head, void (*func)(long)) { // функция map
    List* _head_ = *head;
    if (_head_ == NULL) return;

    long value = _head_->value;
    func(value);
    m(&_head_->next, func);
}

void f(List **head, long (*func)(long), List **newList) { // функция filter
    List* _head_ = *head;
    if (_head_ == NULL) return;
    long value = _head_->value;
    if (func(value)) {
        *newList = add_element(value, newList);
    }
    f(&_head_->next, func, newList);
}

void deleteList(List **head) {
    if (head == NULL) return;
    List* prev = NULL;
    while ((*head)->next) {
        prev = (*head);
        (*head) = (*head)->next;
        free(prev);
    }
    free(*head);
}

int main() {
    List* head = NULL;
    while (data_length-- != 0) {
       head = add_element(data[data_length], &head);
    }
    m(&head, print_int);
    puts(&empty_str);
    List* newList = NULL;
    f(&head, p, &newList);
    m(&newList, print_int);
    puts(&empty_str);

    deleteList(&head);
    deleteList(&newList);
    return 0;
}