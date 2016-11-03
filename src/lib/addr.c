#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "proctal.h"
#include "internal.h"
#include "linux.h"

struct proctal_addr_iter {
	// Proctal instance.
	proctal p;

	// Start address of the next call.
	void *curr_addr;

	// Memory mappings of the address space.
	FILE *maps;

	// Current region being read.
	struct proctal_linux_mem_region region;

	// Tells which regions are iterated.
	long region_mask;

	// Address alignment.
	size_t align;

	// Size of the value of the address. We only want to return addresses
	// that when dereferenced can return values of up to this size.
	size_t size;
};

static inline int has_started(proctal_addr_iter iter)
{
	return iter->curr_addr != NULL;
}

static inline int has_ended(proctal_addr_iter iter)
{
	return iter->curr_addr != NULL && iter->maps == NULL;
}

static inline int has_reached_region_end(proctal_addr_iter iter)
{
	return ((void *) ((char *) iter->curr_addr + iter->size)) > iter->region.end_addr;
}

static inline void *align_addr(void *addr, size_t align)
{
	ptrdiff_t offset = ((unsigned long) addr % align);

	if (offset != 0) {
		offset = align - offset;
	}

	return (void *) ((char *) addr + offset);
}

static inline int interesting_region(proctal_addr_iter iter)
{
	if (iter->region_mask == 0) {
		return 1;
	}

	if (iter->region_mask & PROCTAL_ADDR_REGION_STACK) {
		if (strncmp(iter->region.path, "[stack", 6) == 0) {
			return 1;
		}
	}

	if (iter->region_mask & PROCTAL_ADDR_REGION_HEAP) {
		if (strcmp(iter->region.path, "[heap]") == 0) {
			return 1;
		}
	}

	return 0;
}

static inline int next_region(proctal_addr_iter iter)
{
	for (;;) {
		if (proctal_linux_read_mem_region(&iter->region, iter->maps) != 0) {
			return 0;
		}

		if (!interesting_region(iter)) {
			continue;
		}

		iter->curr_addr = align_addr(iter->region.start_addr, iter->align);

		// After applying the correct alignment to the address, it is
		// possible to have reached the end of the memory region. Even
		// if this is very unlikely to happen, this situation must be
		// checked.
		if (!has_reached_region_end(iter)) {
			break;
		}
	}

	return 1;
}

static inline int next_address(proctal_addr_iter iter)
{
	if (iter->curr_addr == NULL) {
		return 0;
	}

	iter->curr_addr = (void *) ((char *) iter->curr_addr + iter->align);

	if (has_reached_region_end(iter) && !next_region(iter)) {
		return 0;
	}

	return 1;
}

static inline int start(proctal_addr_iter iter)
{
	iter->maps = fopen(proctal_linux_proc_path(proctal_pid(iter->p), "maps"), "r");

	if (iter->maps == NULL) {
		proctal_set_error(iter->p, PROCTAL_ERROR_PERMISSION_DENIED);
		return 0;
	}

	if (!next_region(iter)) {
		fclose(iter->maps);
		iter->maps = NULL;
		return 0;
	}

	return 1;
}

proctal_addr_iter proctal_addr_iter_create(proctal p)
{
	proctal_addr_iter iter = proctal_alloc(p, sizeof *iter);

	if (iter == NULL) {
		return iter;
	}

	iter->p = p; 
	iter->curr_addr = NULL;
	iter->maps = NULL;
	iter->region_mask = PROCTAL_ADDR_REGION_HEAP | PROCTAL_ADDR_REGION_STACK;
	iter->size = 1;
	iter->align = 1;

	return iter;
}

void proctal_addr_iter_destroy(proctal_addr_iter iter)
{
	proctal_dealloc(iter->p, iter);
}

size_t proctal_addr_iter_size(proctal_addr_iter iter)
{
	return iter->size;
}

void proctal_addr_iter_set_size(proctal_addr_iter iter, size_t size)
{
	iter->size = size > 0 ? size : 1;
}

size_t proctal_addr_iter_align(proctal_addr_iter iter)
{
	return iter->align;
}

void proctal_addr_iter_set_align(proctal_addr_iter iter, size_t align)
{
	iter->align = align > 0 ? align : 1;
}

long proctal_addr_iter_region(proctal_addr_iter iter)
{
	return iter->region_mask;
}

void proctal_addr_iter_set_region(proctal_addr_iter iter, long mask)
{
	iter->region_mask = mask;
}

int proctal_addr_iter_next(proctal_addr_iter iter, void **addr)
{
	if (!has_started(iter)) {
		if (!start(iter)) {
			return 0;
		}

		*addr = iter->curr_addr;
		return 1;
	} else if (has_ended(iter)) {
		return 0;
	}

	if (next_address(iter)) {
		*addr = iter->curr_addr;
		return 1;
	} else {
		fclose(iter->maps);
		return 0;
	}
}