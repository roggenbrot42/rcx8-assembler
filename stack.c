#include<stdio.h>
#include<stdlib.h>

#include "stack.h"

size_t stack_init(stack_t *st, size_t size){
	if(size < 1) return -1;
	
	st->elements = (stack_elem *) malloc(sizeof(stack_elem)*size);
	if(st->elements == NULL) return -1;
	
	st->len = size;
	st->cur = -1;
	
	return size;
}

void stack_destroy(stack_t *st){
	if(st->elements)
		free(st->elements);
	
	st->len = 0;
	st->cur = -1;
}

int stack_push(stack_t *st, stack_elem e){
	if(stack_is_full(st)) return -1;
	
	st->elements[++st->cur] = e;
	return 0;
}

stack_elem stack_pop(stack_t * st){
	if(stack_is_empty(st)) return -1;
	
	return st->elements[st->cur--];
}

stack_elem stack_peek(stack_t *st){
	if(stack_is_empty(st)) return -1;
	
	return st->elements[st->cur];
}

inline int stack_is_empty(stack_t *st){
	return st->cur < 0;
}

inline int stack_is_full(stack_t *st){
	return st->cur >= st->len - 1;
}