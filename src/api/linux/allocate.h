#ifndef API_LINUX_ALLOCATE_H
#define API_LINUX_ALLOCATE_H

#include "api/linux/proctal.h"

void *proctal_linux_allocate(struct proctal_linux *pl, size_t size, int permissions);

void proctal_linux_deallocate(struct proctal_linux *pl, void *addr);

#endif /* API_LINUX_ALLOCATE_H */
