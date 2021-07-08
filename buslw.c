/** @file buslw.c
 * Main function for BUSL, Windows version.
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

#include <stdio.h>
#include <string.h>
#include <malloc.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN

#if !defined(MB_SETFOREGROUND)
#define MB_SETFOREGROUND 0
#endif

#if !defined(MB_TOPMOST)
#define MB_TOPMOST 0
#endif

#include "busl.h"

static const char SPACES[] = " \t";

typedef struct msgdata {
	char *msgtxt;
	int msglen;
} msgdata;

static void __stdcall wrt(void *output, const char *str) {
	msgdata *d = (msgdata *) output;
	char *p;

	if (!d->msglen) {
		d->msglen = 1024;
		d->msgtxt = p = (char *) malloc(d->msglen);
	} else {
		p = &d->msgtxt[strlen(d->msgtxt)];
		if ((p-d->msgtxt) > (d->msglen - 512)) {
			d->msglen += 512;
			d->msgtxt = (char *) realloc(d->msgtxt, d->msglen);
			p = &d->msgtxt[strlen(d->msgtxt)];
		}
	}

	sprintf(p, "%s", str);
}

/**
 * Main function of application, Windows version.
 *
 * The code for the parsing of the command line arguments is borrowed
 * from the file tkAppInit.c in Tcl/Tk 8.3.
 *
 * Parse the Windows command line string into argc/argv.  Done here
 * because we don't trust the builtin argument parser in crt0.
 * Windows applications are responsible for breaking their command
 * line into arguments.
 *
 * 2N backslashes + quote -> N backslashes + begin quoted string
 * 2N + 1 backslashes + quote -> literal
 * N backslashes + non-quote -> literal
 * quote + quote in a quoted string -> single quote
 * quote + quote not in quoted string -> empty string
 * quote -> begin quoted string
 *
 * @param inst instance of current application
 * @param previnst not used
 * @param cmdline command line
 * @param cmdshow not used
 */
int WINAPI WinMain(HINSTANCE inst, HINSTANCE previnst, LPSTR cmdline, int cmdshow) {
	LPSTR p;
	char *arg, *argSpace, **argv, argv0[256];
	int inquote, copy, slashes;
	size_t argc, size;

	int changed = 0;
	Busl s;
	msgdata d = {0, 0};

	previnst = previnst; /* prevent compiler warning */
	cmdshow = cmdshow; /* prevent compiler warning */
	GetModuleFileName(inst, argv0, sizeof(argv0));
	/* If the filename ends with ".exe", strip this */
	arg = strrchr(argv0, '.');
	if (arg && !strcmp(arg, ".exe"))
		*arg = '\0';
	/* If the filename contains '\\', strip everything before it */
	arg = strrchr(argv0, '\\');
	if (arg)
		arg++;
	else
		arg = argv0;

	/*
	 * Precompute an overly pessimistic guess at the number of arguments
	 * in the command line by counting non-space spans.
	 */

	size = 3;
	for (p = cmdline; *p != '\0'; p++) {
		if (*p && strchr(SPACES, *p)) {
			size++;
			while (*p && strchr(SPACES, *p)) {
				p++;
			}
			if (*p == '\0') {
				break;
			}
		}
	}
	argSpace = (char *) malloc(size * sizeof(char *) + lstrlen(cmdline) + 1);
	argv = (char **) argSpace;
	argSpace += size * sizeof(char *);
	size--;

	p = cmdline;
	argv[0] = arg;
	for (argc = 1; argc < size; argc++) {
		argv[argc] = arg = argSpace;
		while (*p && strchr(SPACES, *p))
			p++;
		if (*p == '\0')
			break;

		inquote = 0;
		slashes = 0;
		for (;;) {
			copy = 1;
			while (*p == '\\') {
				slashes++;
				p++;
			}
			if (*p == '"') {
				if ((slashes & 1) == 0) {
					copy = 0;
					if ((inquote) && (p[1] == '"')) {
						p++;
						copy = 1;
					} else {
						inquote = !inquote;
					}
				}
				slashes >>= 1;
			}

			while (slashes) {
				*arg = '\\';
				arg++;
				slashes--;
			}

			if ((*p == '\0') || (!inquote && strchr(SPACES, *p))) {
				break;
			}
			if (copy != 0) {
				*arg = *p;
				arg++;
			}
			p++;
		}
		*arg = '\0';
		argSpace = arg + 1;
	}
	argv[argc] = NULL;


	busl_create(&s, wrt, &d);
	if (argc<2) {
		changed = busl_usage(&s, argv[0]); /* prevent de "no sources modified" message */
	}
	arg = argv[0];
	while (--argc) {
		changed |= busl_beautify(&s, *(++argv));
	}
	busl_finish(&s, changed);
	if (d.msgtxt) {
		MessageBeep(MB_ICONEXCLAMATION);
		MessageBox((HWND) 0, d.msgtxt, arg, MB_OK|MB_ICONEXCLAMATION|MB_SYSTEMMODAL|MB_SETFOREGROUND|MB_TOPMOST);
		free(d.msgtxt);
	}
	return s.result;
}
