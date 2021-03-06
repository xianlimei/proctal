#include <stdio.h>

#include "cli/cmd/dump.h"
#include "cli/printer.h"
#include "api/include/proctal.h"
#include "chunk/chunk.h"

int cli_cmd_dump(struct cli_cmd_dump_arg *arg)
{
	proctal_t p = proctal_open();

	if (proctal_error(p)) {
		cli_print_proctal_error(p);
		proctal_close(p);
		return 1;
	}

	proctal_pid_set(p, arg->pid);

	if (arg->freeze) {
		proctal_freeze(p);

		if (proctal_error(p)) {
			cli_print_proctal_error(p);
			proctal_close(p);
			return 1;
		}
	}

	if (!arg->read && !arg->write && !arg->execute) {
		// By default will dump everything.
		proctal_scan_region_read_set(p, 0);
		proctal_scan_region_write_set(p, 0);
		proctal_scan_region_execute_set(p, 0);
	} else {
		proctal_scan_region_read_set(p, arg->read);
		proctal_scan_region_write_set(p, arg->write);
		proctal_scan_region_execute_set(p, arg->execute);
	}

	long mask = 0;

	if (arg->program_code) {
		mask |= PROCTAL_REGION_PROGRAM_CODE;
	}

	proctal_scan_region_mask_set(p, mask);

	proctal_scan_region_start(p);

	const size_t output_block_size = 1024 * 1024 * 2;
	char *output_block = malloc(output_block_size);

	void *start, *end;

	struct chunk chunk;

	while (proctal_scan_region(p, &start, &end)) {
		chunk_init(&chunk, start, end, output_block_size);

		do {
			char *offset = chunk_offset(&chunk);
			size_t size = chunk_size(&chunk);

			proctal_read(p, offset, output_block, size);

			if (proctal_error(p)) {
				cli_print_proctal_error(p);

				proctal_error_recover(p);

				// Let's try the next chunk.
				continue;
			}

			fwrite(output_block, 1, size, stdout);
		} while (chunk_next(&chunk));
	}

	free(output_block);

	proctal_scan_region_stop(p);

	if (arg->freeze) {
		proctal_unfreeze(p);
	}

	proctal_close(p);

	return 0;
}
