/***************************************************************************

  machine.c

  Functions to emulate general aspects of the machine (RAM, ROM, interrupts,
  I/O ports)

***************************************************************************/

#include <stdarg.h>
#include "driver.h"
#include "cpu/z80/z80.h"

void    jupiter_init_machine(void) {
	fprintf (stderr, "jupiter_init\n");
	fflush (stderr);
}

void    jupiter_stop_machine(void) {
}

int     jupiter_rom_id(const char *name, const char *gamename) {
	return 0;
}

