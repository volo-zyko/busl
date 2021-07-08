/** @file busl.h
 * Beautifier application for Universal Set of Languages.
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

#ifndef BUSL_H
#   define BUSL_H

#   ifdef __cplusplus
#       include <iostream>
extern "C" {
#   endif

#   if defined(_DOS) || defined(_WIN16)
#       define __stdcall __pascal
#   else
#       if !defined(__stdcall) && !defined(_WIN32) && !defined(_WIN64)
#           define __stdcall
#       endif
#   endif

	enum {
		/** Beautifier resulted in a source-code change */
		CHANGED = 1,
		/** Use file extension to determine format */
		AUTOMODE = 2, /* defaultflags only */
		/** Don't give warnings, only errors */
		QUIETMODE = 4, /* defaultflags only */
		/** UNIX linefeed mode */
		UNIXLFMODE = 8, /* defaultflags only */
		/** MAC carriage return mode */
		MACCRMODE = 16, /* defaultflags only */
		/** strip spaces */
		SPACESTRIP = 2, /* flags only */
		/** insert space before next token */
		SPACENEEDED = 4, /* flags only */
		/** output space as is */
		SPACEASIS = 8, /* flags only */
		/** insert linefeed before next token (if not already there) */
		SPACENEEDLF = 16, /* flags only */
		/** combination of 'SPACE' flags */
		SPACEHANDLING = (SPACESTRIP|SPACENEEDED|SPACEASIS|SPACENEEDLF),
		/** output files will be written */
		NOTESTMODE = 32, /* defaultflags only */
		/** Assume source code is embedded in XML/HTML/SGML */
		XMLMODE = 64,
		/** Prepare as ZIP */
		ZIPMODE = 128,
		/** Strip unnessary spaces and comments */
		STRIPMODE = 256,
		/** Last character was a backslash. Only used between quotes */
		BACKSLASH = 512,
		/** Last character was a * (C-comment) or %, # or ? (XML): This might be almost the end of a comment block */
		ALMOSTEND = 1024,
		/** Potential extra indent */
		EXTRAINDENT = 2048
	};

	enum {
		/** Size of input and output buffer */
		BUFSIZE = 8192,
		/** Size of level stack */
		STACKSIZE = 256
	};

	/**
	 * Remember files that should be removed before BUSL finishes
	 */
	typedef struct toberemoved {
		/** next in list */
		struct toberemoved *next;
		/** file name */
		char name[8];
	} toberemoved;

#   ifndef BUSL_EXPORT
#       define BUSL_EXPORT
#   endif

	struct Busl;
	BUSL_EXPORT struct Busl *__stdcall busl_create(struct Busl *s, void (__stdcall* wrt)(void *, const char *), void *output);
	BUSL_EXPORT int __stdcall busl_usage(struct Busl *s, const char *argv0);
	BUSL_EXPORT int __stdcall busl_beautify(struct Busl *s, const char *filename);
	BUSL_EXPORT int __stdcall busl_finish(struct Busl *s, int changed);
	BUSL_EXPORT void __stdcall busl_delete(struct Busl *s);

	typedef struct Busl {
#   ifdef __cplusplus
	private:
		struct Busl &operator=(const struct Busl &);
	public:
		__stdcall Busl() {
			busl_create(this, wrtfn, (void *) &std::cerr);
		}
		static void __stdcall wrtfn(void *output, const char *str) {
			(*(std::ostream *) output) << str;
		}
		bool __stdcall usage(const char *argv0) {
			return busl_usage(this, argv0)>0;
		}
		bool __stdcall beautify(const char *filename) {
			return busl_beautify(this, filename)>0;
		}
		int __stdcall finish(bool changed) {
			return busl_finish(this, changed? 1: 0);
		}
		/** optional storage */
		std::ostream *output;
	private:
#   else
		void *output;
#   endif

		/** function to be used for message output */
		void (__stdcall *wrt)(void *, const char *);
		/** list of files to be removed at program end */
		toberemoved *first;
		/** flags are reset to this value at every file begin */
		int defaultflags;
		/** number of spaces used for indenting (<0 = use tabs) */
		int tabs;
		/** Current quoting mode. */
		/*  \0  no quoting
		 *  \n  C++/C#/java, shell-type or VB comment
		 *  *   C-style comments
		 *  +   D-style comments
		 *  <   XML section (%, #, ?)
		 *  /   regexp
		 *  '   string
		 *  "   string
		 *  `   string
		 */
		char quoted;
		/** Quoting mode within comments. Only used to determine whether space-characters
		 * (e.g. tabs) should be converted to spaces or escape sequences or left as-is. */
		/*  \0 no quoting
		 *  /  regexp
		 *  '  string
		 *  "  string
		 *  `  string
		 */
		char commentquoted;

		char inbuf[BUFSIZE];
		char outbuf[BUFSIZE];
		/** position of opening bracket in outbuf for each indent level */
		int indentpos[STACKSIZE];
		/** store indent level type. */
		/*  )}]    as you would expect
		 *  (      same as ')', but follows if/for/while
		 *  :;     used for ?: ternary operator
		 *  space  additional indent for case statements.
		 */
		char indentstack[STACKSIZE];
		/** flags for each indent level */
		int indentflags[STACKSIZE];

		/** current position in input buffer */
		int inpos;
		/** current position in output buffer */
		int outpos;
		/** output directory */
		const char *outdir;
		/** the number of characters to be stripped in C-type comment */
		int numstrip;
		/** various flags during beautify process */
		int flags;
		/** indent level at this moment */
		int indent;
		/** keeps track of output line number */
		int linenum;
		/** indent level at start of current line */
		int curindent;
		/** return value for main program
		 * EXIT_SUCCESS = OK
		 * EXIT_FAILURE = errors
		 * 2*EXIT_FAILURE = only warnings
		 */
		int result;
	} Busl;

#   ifdef __cplusplus
}
#   endif

#endif /* BUSL_H */
