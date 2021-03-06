#include "cli/cmd/measure.h"
#include "cli/printer.h"
#include "api/include/proctal.h"

int cli_cmd_measure(struct cli_cmd_measure_arg *arg)
{
	size_t list_size = cli_val_list_size(arg->value_list);
	size_t total_size = 0;

	for (size_t i = 0, j = 0; i < arg->array; ++i, ++j) {
		if (j == list_size) {
			j = 0;
		}

		cli_val v = cli_val_list_get(arg->value_list, j);

		total_size += cli_val_sizeof(v);
	}

	printf("%lu\n", total_size);

	return 0;
}
