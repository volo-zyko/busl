/** @file busl.c
 * Main function for BUSL, console version.
 * Copyright (c) 2003-2009, Jan Nijtmans. All rights reserved.
 */

/* This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#if defined(__MACOS__) && defined(__MWERKS__)
#   include <console.h>
#endif
#include <stdio.h>
#include <string.h>
#include "busl.h"

static void __stdcall wrt(void *data, const char *str) {
	fprintf((FILE *) data, "%s", str);
}

/**
 * Main function of application
 *
 * @param argc number of command line arguments
 * @param argv command line arguments
 */
int main(int argc, char *argv[]) {
	int changed = 0;
	Busl s;
	busl_create(&s, wrt, (void *) stderr);

#if defined(__MACOS__) && defined(__MWERKS__)
	argc = ccommand(&argv); /* is this correct? */
#endif
	if (argc<2) {
		changed = busl_usage(&s, *argv); /* prevent de "no sources modified" message */
	}
	while (--argc) {
		changed |= busl_beautify(&s, *(++argv));
	}
	return busl_finish(&s, changed);
}
