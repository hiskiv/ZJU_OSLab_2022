#ifndef _VM_H_
#define _VM_H_

#include "types.h"
#include "stdint.h"

void setup_vm(void);
void setup_vm_final(void);
void create_mapping(uint64*, uint64, uint64, uint64, int);
int has_mapping(pagetable_t, uint64_t);

#endif