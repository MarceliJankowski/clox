#ifndef VM_H
#define VM_H

#include <stdint.h>

#include "chunk.h"
#include "value.h"

#define VM_STACK_INITIAL_CAPACITY 256

typedef struct {
  Chunk *chunk;
  uint8_t *ip;
  struct {
    Value *values;
    long capacity, count;
  } stack;
} VirtualMachine;

void vm_init(void);
void vm_free(void);
void vm_interpret(Chunk *chunk);
void vm_stack_push(Value value);
Value vm_stack_pop(void);

#endif // VM_H