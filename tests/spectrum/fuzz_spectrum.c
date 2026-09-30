/**
 * @file    fuzz_spectrum.c
 * @brief   libFuzzer entry point for the spectrum widget fit-selection path
 */

#include <gtk/gtk.h>
#include <stdint.h>
#include <stdlib.h>

#include "harness.h"


int LLVMFuzzerInitialize(int *argc, char ***argv)
{
	(void) argc;
	(void) argv;

	if (harness_init() < 0)
		return -1;

	return 0;
}


int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	harness_run(data, size);

	return 0;
}
