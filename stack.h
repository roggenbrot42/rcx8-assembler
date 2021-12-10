#ifndef STACK_H
#define STACK_H

typedef char stack_elem;

typedef struct {
	int cur;  
	int len;
	stack_elem * elements;
} stack_t;

size_t stack_init(stack_t*, size_t);
void stack_destroy(stack_t*);
int stack_push(stack_t*, stack_elem);
stack_elem stack_pop(stack_t *);
stack_elem stack_peek(stack_t *);
int stack_is_empty(stack_t*);
int stack_is_full(stack_t*);

#endif