#ifndef _SYSCALL_H_
#define _SYSCALL_H_

#include "stdint.h"
#include "proc.h"

#define SYS_WRITE   64
#define SYS_GETPID  172

long sys_write(unsigned int fd, const char* buf, int count);

long sys_getpid();

#endif