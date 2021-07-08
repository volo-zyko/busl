/** @file busl.c
 * Beautifier application for Universal Set of Languages.
 */

/** Copyright message */
static const char COPYRIGHT[] =
"BUSL version 0.91: Beautifier for Universal Set of Languages.\n\
Copyright (c) 2003-2009, Jan Nijtmans. All rights reserved.\n";

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
#include <stdlib.h>
#include <string.h>

#if defined(_DOS) || defined(_WIN16)
#   define usleep(us) /* Dont't bother to implement this on DOS or Win16 */
#   include <io.h>
#   define unlink _unlink
#elif defined(_WIN32) || defined(_WIN64)
/* Don't include <windows.h> just for this single function */
extern __declspec(dllimport) void __stdcall Sleep(unsigned long int ms);
#   define usleep(us) Sleep(us/1000)
#   include <io.h>
#else
#   include <unistd.h>
#endif

#include "busl.h"

static const char ERRORMESSAGE[] = "Please correct this and try again. (beautified code stored in %s)\n";

static const char SPACES[] = "\t ";
static const char ISCOMMENTORXML[] = "<\n*+";
static const char ISCOMMENT[] = "\n*+";
static const char ISCCOMMENT[] = "*+";
static const char ISGTORCTRLZ[] = "\032>";
static const char BEFORETOKEN[] = "\t (),:;[]{}";
static const char DIRSEPARATOR[] = "/:\\";
/* Due to lack of fantasy, the symbols below don't have a proper name yet */
static const char XXXX10[] = " ,:;"; /* stack */
static const char XXXX11[] = " :;"; /* stack */
static const char XXXX12[] = ":;"; /* stack */
static const char XXXX13[] = " ;"; /* stack */
static const char XXXX14[] = "(,:;=?[{"; /* buf */
static const char XXXX15[] = "(,:;=[{"; /* buf */
static const char XXXX16[] = "!(,:;<=>?[{~"; /* buf */
static const char XXXX17[] = "%&*+-/^|~"; /* buf */
static const char XXXX18[] = "!$(@[{~"; /* buf */

/** begin &lt;script&gt; tag name */
static const char scripttag[] = "<script";

/** end &lt;/script&gt; tag name */
static const char endscripttag[] = "</script";

/** file extensions which disable the "s" option */
static const char *const nostripext[] = {
	"c", "cpp", "cs", "cxx", "d", "h", "hpp", "java", "jsp", "nice", "pcc"
};

/** file extensions resulting in generic mode */
static const char *const genext[] = {
	"as", "bsh", "c", "cpp", "cs", "cxx", "d", "groovy", "gy", "h", "hpp", "java", "js", "jud", "judo", "ksh", "nice", "os", "pcc", "pnut", "sh", "tcl"
};

/** file extensions resulting in xml/html/sgml mode */
static const char *const xmlext[] = {
	"asax", "asp", "aspx", "csproj", "htm", "html", "jsp", "php", "shtml", "vcproj", "xml"
};

/** file extensions which should be ignored */
static const char *const ignorext[] = {
	"a", "bat", "bmp", "boo", "bz2", "class", "css", "def", "dll", "dsp", "dsw", "exe", "gif", "gz", "ico", "lzma", "msp", "ncb", "o", "obj", "opt", "png", "ilk", "jar", "jpg", "la", "lo", "lib", "MAK", "MF", "pl", "plg", "py", "rb", "sln", "tgz", "zip"
};

/**
 * Check if a file extension matches one of a list
 *
 * @param ext file extension to be checked
 * @param table table to be matched against
 * @param size of table (in bytes!)
 * @return 1 when matches, 0 when not
 */
static int __stdcall checkext(const char *ext, const char *const *table, int size) {
	const char *const *q;
	size /= (int) sizeof(const char *);
	q = &table[size];
	while ((--q>=table) && strcmp(*q, ext)) {}
	return q>=table;
}

/**
 * Check if the output buffer contains some key word
 *
 * @param Busl BUSL status
 * @param key key word
 * @return 1 when found, 0 when not
 */
static int __stdcall checkkey(const Busl *s, const char *key) {
	const char *begin = &s->outbuf[s->outpos-strlen(key)];
	if ((begin<s->outbuf) || ((begin>s->outbuf) && !strchr(BEFORETOKEN, begin[-1]))) {
		return 0;
	}
	while (*key && (*begin==*key)) {
		begin++;
		key++;
	}
	return !*key;
}

/**
 * Try to remove a file. If this fails for whatever reason,
 * remember this so we can try again later.
 *
 * @param Busl BUSL status
 * @param filename filename
 */
static void __stdcall removefile(Busl *s, const char *filename) {
	if (unlink(filename)) {
		struct toberemoved *f = (struct toberemoved *) malloc(sizeof(struct toberemoved) + strlen(filename) - 7);
		f->next = s->first;
		strcpy(f->name, filename);
		s->first = f;
	}
}

/**
 * Generate a warning message
 *
 * @param Busl BUSL status
 * @param format format string
 * @param msg message to be included in error message
 * @param linenum line number to be included in error message
 * @param c character to be included in error message
 */
static int __stdcall warning(Busl *s, const char *format, const char *msg, int linenum, char c) {
	char *p = strchr(format, ':');
	char buffer[BUFSIZE];
	if (s->result < 0) {
		s->result = EXIT_SUCCESS;
		s->wrt(s->output, COPYRIGHT);
	}
	if (p) {
		if (!memcmp(p, ": WARNING", 9)) {
			if (s->result==EXIT_SUCCESS) s->result = 2*EXIT_FAILURE;
		} else if (!memcmp(p, ": ERROR", 7)) {
			s->result = EXIT_FAILURE;
		}
	}

	if (linenum>0) {
		sprintf(buffer, format, msg, linenum, (int) s->outpos, c);
	} else {
		sprintf(buffer, format, msg, c);
	}
	s->wrt(s->output, buffer);
	return s->flags;
}

/**
 * Insert as many tabs/spaces in the line buffer as the indent level indicates
 *
 * @param Busl BUSL status
 */
static void __stdcall writeindent(Busl *s) {
	int count;
	if (!(s->flags&STRIPMODE)) {
		if (s->tabs>=0) {
			/* use spaces for indenting */
			count = s->tabs*s->indent;
			while (count--) {
				s->outbuf[s->outpos++] = ' ';
			}
		} else {
			/* use tabs for indenting */
			count = s->indent;
			while (count--) {
				s->outbuf[s->outpos++] = '\t';
			}
		}
	}
}

/**
 * Write a single char to output. If the line is not within comment or quotes, indenting
 * is applied. If there are more than a single unclosed opening bracket in the line, then
 * the line is split into multiple lines. In addition, the written output line is compared
 * with the input line. If they are different the flag CHANGED is set.
 *
 * @param Busl BUSL status
 * @param c character to be written
 * @param fout FILE to be written
 */
static void __stdcall writechar(Busl *s, int c, FILE *fout) {
	if (c=='\n') {
		int begwrite = 0;
		int saveindent = s->indent;
		int endwrite;
		++s->linenum;
		s->outbuf[s->outpos++] = (char) c;
		if (!(s->flags&STRIPMODE)) {
			if (*s->outbuf == '#' && s->tabs) {
				/* If line starts with #region or #endregion, indent it normally. */
				const char *p = s->outbuf;
				if (!memcmp(++p, "end", 3)) {
					p += 3;
				}
				if (!memcmp(p, "region", 6)) {
					int indent = (s->tabs>0)? s->tabs*saveindent: saveindent;
					memmove(&s->outbuf[indent], s->outbuf, s->outpos);
					memset(s->outbuf, (s->tabs>0)? ' ': '\t', indent);
					s->outpos += indent;
				}
			}
			while (s->curindent<saveindent-1) {
				/* There are to many unclosed opening brackets on this line,
				 * so this line must be split */
				int saveoutpos = s->outpos;
				int splitoutpos;
				if (strchr(XXXX10, s->indentstack[s->curindent++])) {
					continue;
				}
				s->indent = s->curindent;
				splitoutpos = endwrite = s->indentpos[s->indent];
				s->flags |= CHANGED;
				while (endwrite>0 && strchr(SPACES, s->outbuf[endwrite-1]))
					--endwrite;
				if (fout) {
					fwrite(&s->outbuf[begwrite], 1, endwrite-begwrite, fout);
					if (s->defaultflags&MACCRMODE) {
						fwrite("\r", 1, 1, fout);
						if (s->defaultflags&UNIXLFMODE) {
							fwrite("\n", 1, 1, fout);
						}
					} else {
						fwrite("\n", 1, 1, fout);
					}
				}
				begwrite = splitoutpos;
				++s->linenum;
				writeindent(s);
				if (fout) {
					fwrite(&s->outbuf[saveoutpos], 1, s->outpos-saveoutpos, fout);
				}
				s->outpos = saveoutpos;
			}
		}
		s->curindent = s->indent = saveindent;
		if ((!(s->flags&STRIPMODE)) || (s->outpos>1) || !strchr(ISCOMMENT, s->quoted)) {
			if (!(s->flags&CHANGED) && ((s->outpos!=s->inpos) || memcmp(s->outbuf, s->inbuf, s->outpos))) {
				s->flags |= CHANGED;
			}
			if ((s->defaultflags&MACCRMODE) && s->outpos) {
				s->outbuf[s->outpos-1] = '\r';
				if (s->defaultflags&UNIXLFMODE) {
					s->outbuf[s->outpos++] = (char) c;
				}
			}
			if (fout) {
				fwrite(&s->outbuf[begwrite], 1, s->outpos-begwrite, fout);
			}
		} else {
			s->flags |= CHANGED;
		}
		s->outpos = s->inpos = 0;
	} else {
		if ((!(s->flags&STRIPMODE)) || strchr("\"#%'/<?`", s->quoted)) {
			if (!(s->outpos || (s->quoted && (s->quoted!='*') && (s->quoted!='+')))) {
				writeindent(s);
			}
			s->outbuf[s->outpos++] = (char) c;
		}
	}
}

/**
 * Beautify the given file. If the given filename does not exist, interpret the
 * characters as options.
 * During beautify a new file &lt;filename&gt;$ is written.
 * If there are no differences detected between input and output, the file
 * &lt;filename&gt;$ is removed.
 * If there are differences, then the file &lt;filename&gt; is copied to &lt;filename&gt;~
 * and &lt;filename&gt;$ is copied to &lt;filename&gt;. If this copy-operation fails (e.g. because)
 * &lt;filename&gt; is read-only, then a warning is given and &lt;filename&gt;$ is maintained.
 *
 * @param Busl BUSL status
 * @param filename filename
 */
int __stdcall busl_beautify(Busl *s, const char *filename) {
	char dest[256];
	char orig[256];
	FILE *fin = 0;
	FILE *fout = 0;
	int c;
	int savechar = '\n'; /* '\n' stands for empty */
	char cmdtype = 0; /* stores type of XML processing command: '%', '#' or '?' */
	const char *p = filename;

	/* If filename starts with @, read command line options from this file */
	if (filename[0] == '@') {
		fin = fopen(&filename[1], "r");
		if (!fin) {
			return warning(s, "%s: WARNING: file not found (ignored).\n", &filename[1], 0, 0);
		}
		c = 0;
		while (fgets(orig, sizeof(orig)-1, fin)!=NULL) {
			char *q = orig;
			while (*q) ++q;
			while (q>=&orig[1] && strchr(" \t\r\n", q[-1])) {
				--q;
			}
			*q = '\0';
			if (*orig && (*orig != '#')) {
				c |= busl_beautify(s, orig);
			}
		}
		fclose(fin);
		return c;
	}
	/* If filename ends with slash, consider it as directory */
	while (*p) ++p;
	if (p>=&filename[2] && strchr(DIRSEPARATOR, p[-1])) {
		s->outdir = (p>&filename[2] || *filename!='.')? filename: (const char *) 0;
		return s->flags;
	}
	s->linenum = 1;
	s->indent = s->inpos = s->outpos = 0;
	s->indentflags[0] = 0;
	fin = fopen(filename, "rb");
	if (!fin) {
		int savetabs = s->tabs;
		int saveflags = s->defaultflags;
		p = filename;
		while (*p) {
			char c = *p;
			if (c >= 'A' && c <= 'Z')
				c += 'a' - 'A';
			if ((c>='0') && (c<='9')) {
				s->tabs = c-'0';
			} else if (c=='a') {
				s->defaultflags |= AUTOMODE;
			} else if (c=='f') {
				s->defaultflags |= CHANGED;
			} else if (c=='g') {
				s->defaultflags &= ~(XMLMODE|AUTOMODE);
			} else if (c=='l') {
				s->defaultflags |= UNIXLFMODE;
			} else if (c=='q') {
				s->defaultflags |= QUIETMODE;
			} else if (c=='r') {
				s->defaultflags |= MACCRMODE;
			} else if (c=='s') {
				s->defaultflags |= STRIPMODE;
			} else if (c=='t') {
				s->defaultflags &= ~NOTESTMODE;
			} else if (c=='x') {
				s->defaultflags &= ~AUTOMODE;
				s->defaultflags |= XMLMODE;
			} else if (c=='z') {
				s->defaultflags |= ZIPMODE;
			} else if (c=='-') {
				if ((p[1]>'0') && (p[1]<='9')) {
					s->tabs = '0'-*(++p);
				} else {
					s->defaultflags &= ~(CHANGED|UNIXLFMODE|MACCRMODE|QUIETMODE|STRIPMODE|ZIPMODE);
				}
			} else {
				s->tabs = savetabs;
				s->defaultflags = saveflags;
				return warning(s, "%s: WARNING: file not found or invalid option (ignored).\n", filename, 0, 0);
			}
			++p;
		}
		return s->flags;
	}
	p = strrchr(filename, '.');
	if (s->outdir) {
		const char *fwd = filename; /* filename without drive */
#if defined(_DOS) || defined(_WIN16) || defined(_WIN32) || defined(_WIN64)
		char drive = *fwd;
		if (((drive>='A' && drive<='Z') || (drive>='a' && drive<='z')) && fwd[1]==':') {
			fwd += 2;
		}
#endif
		if (strchr(DIRSEPARATOR, *fwd)) {
			if (strchr(DIRSEPARATOR, *s->outdir) || s->outdir[1]==':') {
				/* filename and outdir are absolute: D:out/ C:/in/file => D:out/in/file */
				strcpy(dest, s->outdir); /*           /out/ C:/in/file =>  /out/in/file */
				strcat(dest, &fwd[1]);
			} else {
				/* filename is absolute and outdir is relative: out/ C:\in\file => C:\in\out/file */
				const char *q = strrchr(filename, *fwd)+1;
				memcpy(dest, filename, q-filename);
				strcpy(&dest[q-filename], s->outdir);
				strcat(dest, q);
			}
		} else {
			/* filename is relative: D:/out/ C:in/file => D:/out/in/file */
			strcpy(dest, s->outdir);
			strcat(dest, fwd);
		}
	} else {
		strcpy(orig, filename);
		strcpy(dest, filename);
		/* Under Windows, running "busl *.cpp" would beautify *.cpp~ and *.cpp$
		 * as well! As a workaround, use *.cp~ as backup file and *.cp$ as
		 * temporary file. This is useful for DOS and WIN16 as well, which
		 * dont accept extensions longer than 3 characters. Only do this if
		 * the file extension is exactly 3 characters long. If the last
		 * character is already '~' or '$' then no beautification is done,
		 * unless the 'f' flag is supplied
		 * Because it might be that a Windows volume is mounted on a UNIX
		 * box, or reverse, just do this processing always.
		 */
		if (p) {
			if (p[1] && p[2] && p[3] && !p[4]) {
				if (p[3]!='~' || !(s->defaultflags&CHANGED)) {
					orig[(p-filename)+3] = '\0';
				}
				if (p[3]!='$' || !(s->defaultflags&CHANGED)) {
					dest[(p-filename)+3] = '\0';
				}
			}
		} else {
			strcat(orig, ".");
			strcat(dest, ".");
		}
		strcat(orig, "~");
		strcat(dest, "$");
	}
	s->flags = (s->defaultflags&~SPACEHANDLING)|SPACESTRIP;
	if (p) {
		if ((!strcmp(orig, filename)) || (!strcmp(dest, filename)) || (!(s->defaultflags&CHANGED) && checkext(++p, ignorext, sizeof(ignorext)))) {
			s->flags &= ~CHANGED;
			return warning(s, "%s: ERROR: unsupported file extension: not modified.\n", filename, 0, 0);
		}
		if (s->defaultflags&AUTOMODE) {
			if (checkext(p, xmlext, sizeof(xmlext))) {
				s->flags |= XMLMODE;
			} else if (checkext(p, genext, sizeof(genext))) {
				s->flags &= ~XMLMODE;
			}
			if ((s->flags&STRIPMODE) && !(s->defaultflags&CHANGED) && checkext(p, nostripext, sizeof(nostripext))) {
				warning(s, "%s: WARNING: \"s\" option should not be used for this file type.\n", filename, 0, 0);
				warning(s, "(Ignored. Use \"f\" if you are really sure that you want this).\n", filename, 0, 0);
				s->flags &= ~STRIPMODE;
			}
		}
	}
	s->quoted = 0;
	s->numstrip = 0;
	if (s->flags&XMLMODE) {
		if (s->flags&ZIPMODE) {
			warning(s, "%s: WARNING: \"z\" option cannot be combined with xml/html/sgml mode (ignored).\n", filename, 0, 0);
			s->flags &= ~ZIPMODE;
		}
		s->quoted = '<';
		s->flags &= ~SPACEHANDLING;
		s->flags |= SPACEASIS;
	}
	if (s->defaultflags & NOTESTMODE) {
		if (s->defaultflags&(UNIXLFMODE|MACCRMODE)) {
			fout = fopen(dest, "wb");
		} else {
			fout = fopen(dest, "w");
		}
		if (!fout) {
			fclose(fin);
			return warning(s, "%s: ERROR: cannot open for writing.\n", dest, 0, 0);
		}
	}
	/* read one char from input stream. If it is CR then read one character ahead. */
	c = fgetc(fin);
	if (c=='\r') {
		savechar = fgetc(fin);
		c = '\n';
	} else {
		savechar = '\n';
	}
	/*
	 * Here the main loop of BUSL starts.
	 */
	while (c!=EOF) {
		/* Special handling of the <CNRL>-Z character */
		if (c=='\032') {
			savechar = fgetc(fin);
			if (savechar!=EOF) {
				if (s->flags&(XMLMODE|ZIPMODE)) {
					s->flags |= CHANGED;
					warning(s, "%s: WARNING: <CTRL>-Z and everything after it is stripped.\n", filename, 0, 0);
					break;
				}
				if (s->outpos) {
					writechar(s, '\n', fout);
				}
				if (!(s->defaultflags&(UNIXLFMODE|MACCRMODE))) {
					/* re-open file in binary mode */
					fclose(fin);
					fin = fopen(dest, "ab");
					if (!fin) {
						fclose(fout);
						return warning(s, "%s: ERROR: cannot re-open in binary mode for writing trailer.\n", dest, 0, 0);
					}
				}
				s->outbuf[0] = (char) c;
				s->outbuf[1] = (char) savechar;
				s->outpos = (int) fread(&s->outbuf[2], 1, sizeof(s->outbuf)-2, fin);
				if (s->outpos>=0) s->outpos += 2;
				while (s->outpos==sizeof(s->outbuf)) {
					if (fout) {
						fwrite(s->outbuf, 1, sizeof(s->outbuf), fout);
					}
					s->outpos = (int) fread(s->outbuf, 1, sizeof(s->outbuf), fin);
				}
				if ((s->outpos>0) && fout) {
					fwrite(s->outbuf, 1, s->outpos, fout);
				}
				s->inpos = s->outpos = 0;
			}
			break;
		}
		s->inbuf[s->inpos++] = (char) c;
		if (s->quoted) {
			if ((s->quoted=='%') && (c!='%') && (c!='?') && (c!='#')) {
				s->quoted = '<';
			}
			switch (c) {
				case ' ': {
					if ((s->quoted=='<')) {
						if ((s->inpos>=8) && !memcmp(&s->inbuf[s->inpos-8], scripttag, 7)) {
							do {
								writechar(s, c, fout);
								/* read one char from input stream. Use the read ahead character if available. */
								if (savechar!='\n') {
									c = savechar;
									savechar = '\n';
								} else {
									c = fgetc(fin);
								}
								if (c=='\r') {
									/* If it is CR then read one character ahead. */
									savechar = fgetc(fin);
									c = '\n';
								}
								s->inbuf[s->inpos++] = (char) c;
							} while (c!=EOF && !strchr(ISGTORCTRLZ, c));
							if (c!='>') {
								--s->inpos;
								continue;
							}
							s->quoted = cmdtype = 0;
							s->flags &= ~SPACEHANDLING;
							s->flags |= SPACESTRIP;
						}
					}
					if (s->flags&SPACESTRIP) {
						if (s->inpos>s->numstrip) {
							s->flags &= ~SPACEHANDLING;
							s->flags |= SPACEASIS;
							writechar(s, c, fout);
						}
					} else {
						writechar(s, c, fout);
					}
					break;
				}
				case '\a': {/* alert, audible alarm, bell */
					if (strchr(ISCOMMENTORXML, s->quoted)) {
						writechar(s, c, fout);
					} else {
						writechar(s, '\\', fout);
						writechar(s, 'a', fout);
					}
					break;
				}
				case '\b': {/* backspace */
					if (strchr(ISCOMMENTORXML, s->quoted)) {
						writechar(s, c, fout);
					} else {
						writechar(s, '\\', fout);
						writechar(s, 'b', fout);
					}
					break;
				}
				case '\f': {/* formfeed */
					if (strchr(ISCOMMENTORXML, s->quoted)) {
						writechar(s, c, fout);
					} else {
						writechar(s, '\\', fout);
						writechar(s, 'f', fout);
					}
					break;
				}
				case '\t': {/* horizontal tab */
					if ((s->quoted=='<')) {
						if ((s->inpos>=8) && !memcmp(&s->inbuf[s->inpos-8], scripttag, 7)) {
							do {
								writechar(s, c, fout);
								/* read one char from input stream. Use the read ahead character if available. */
								if (savechar!='\n') {
									c = savechar;
									savechar = '\n';
								} else {
									c = fgetc(fin);
								}
								if (c=='\r') {
									/* If it is CR then read one character ahead. */
									savechar = fgetc(fin);
									c = '\n';
								}
								s->inbuf[s->inpos++] = (char) c;
							} while (c!=EOF && !strchr(ISGTORCTRLZ, c));
							if (c=='>') {
								writechar(s, c, fout);
								s->quoted = cmdtype = 0;
								s->flags &= ~SPACEHANDLING;
								s->flags |= SPACENEEDED;
							} else {
								--s->inpos;
								continue;
							}
						}
					} else if (strchr(ISCCOMMENT, s->quoted) && (s->flags&SPACESTRIP) && (s->inpos>s->numstrip)) {
						s->flags &= ~SPACEHANDLING;
						s->flags |= SPACEASIS;
					}

					if (!(s->flags&SPACESTRIP)) {
						if ((s->commentquoted=='`') || strchr("<`", s->quoted)) {
							writechar(s, c, fout);
						} else if (s->commentquoted || !strchr(ISCOMMENT, s->quoted)) {
							writechar(s, '\\', fout);
							writechar(s, 't', fout);
						} else if (!s->tabs) {
							writechar(s, c, fout);
						} else {
							int numtabs = s->numstrip>0? s->numstrip: 0;
							int td = s->tabs;
							if (td<0) td = -td;
							s->flags |= CHANGED;
							s->inbuf[s->inpos-1] = ' '; /* XXX This is suspicious*/
							writechar(s, ' ', fout);
							while ((numtabs<s->inpos) && (s->inbuf[numtabs]=='\t')) numtabs++;
							while ((s->inpos-numtabs)%td) {
								writechar(s, ' ', fout);
								s->inbuf[s->inpos++] = ' '; /* XXX This is suspicious*/
							}
						}
					}
					break;
				}
				case '\v': {/* vertical tab */
					if (strchr(ISCOMMENTORXML, s->quoted)) {
						writechar(s, c, fout);
					} else {
						writechar(s, '\\', fout);
						writechar(s, 'v', fout);
					}
					break;
				}
				case '\n': {
					if (s->quoted=='<') {
						if ((s->inpos>=8) && !memcmp(&s->inbuf[s->inpos-8], scripttag, 7)) {
							do {
								writechar(s, c, fout);
								/* read one char from input stream. Use the read ahead character if available. */
								if (savechar!='\n') {
									c = savechar;
									savechar = '\n';
								} else {
									c = fgetc(fin);
								}
								if (c=='\r') {
									/* If it is CR then read one character ahead. */
									savechar = fgetc(fin);
									c = '\n';
								}
								s->inbuf[s->inpos++] = (char) c;
							} while (c!=EOF && !strchr(ISGTORCTRLZ, c));
							if (c=='>') {
								writechar(s, c, fout);
								s->quoted = cmdtype = 0;
								s->flags &= ~SPACEHANDLING;
								s->flags |= SPACENEEDED;
							} else {
								--s->inpos;
								continue;
							}
						}
					} else if (strchr(ISCOMMENT, s->quoted)) {
						int backslashpos;
						if (!(s->flags&BACKSLASH)) {
							s->flags &= ~SPACEHANDLING;
							s->flags |= SPACESTRIP;
							if ((s->outpos>0) && strchr(SPACES, s->outbuf[s->outpos-1])) {
								s->flags &= ~BACKSLASH;
								s->outbuf[s->outpos--] = 0;
								while ((s->outpos>0) && strchr(SPACES, s->outbuf[s->outpos-1])) --s->outpos;
								backslashpos = s->outpos;
								while ((backslashpos>0) && (s->outbuf[backslashpos-1]=='\\')) --backslashpos;
								if ((backslashpos-s->outpos) &1) {
									s->flags &= ~SPACEHANDLING;
									s->flags |= (BACKSLASH|SPACEASIS);
									if (!(s->defaultflags&QUIETMODE)) {
										warning(s, "%s(%d,%d): WARNING: Backslash followed by space(s) detected in end of comment.\n", filename, s->linenum, 0);
										warning(s, "Assuming this comment is meant to continue at the next line\n%s\n", s->outbuf, 0, 0);
									}
								}
							}
						}
						if (s->flags&XMLMODE && s->flags&STRIPMODE && (s->quoted=='\n')) {
							const char *p;
							char saveinchar = s->inbuf[s->inpos];
							s->inbuf[s->inpos] = 0;
							p = strstr(s->inbuf, "//");
							if (p && strstr(p, "-->")) {
								strcpy(&s->outbuf[s->outpos], "//-->");
								s->outpos += 5;
							}
							s->inbuf[s->inpos] = saveinchar;
						}
					}
					writechar(s, c, fout);
					break;
				}
				case '>': {
					if (s->quoted=='<') {
						if ((s->inpos>=8) && !memcmp(&s->inbuf[s->inpos-8], scripttag, 7)) {
							s->quoted = cmdtype = 0;
						}
					} else if (s->flags&XMLMODE) {
						if (cmdtype) {
							if (s->outpos && (s->outbuf[s->outpos-1]==cmdtype)) {
								if (s->quoted=='\n') {
									s->quoted = '%'; cmdtype = 0;
								} else {
									s->outbuf[s->outpos] = 0;
									warning(s, "%s(%d,%d): WARNING: '%c>' found in string or comment (ignored).\n", filename, s->linenum, cmdtype);
									warning(s, "%s%c...\n", s->outbuf, 0, (char) c);
								}
							}
						} else if ((s->inpos>=9) && !memcmp(&s->inbuf[s->inpos-9], endscripttag, 8)) {
							if (s->quoted=='\n') {
								s->quoted = '%'; cmdtype = 0;
								if (s->flags&STRIPMODE) {
									const char *p;
									char saveinchar = s->inbuf[s->inpos];
									s->inbuf[s->inpos] = 0;
									p = strstr(s->inbuf, "//");
									if (p && strstr(p, "-->")) {
										if (s->outpos && s->outbuf[s->outpos-1]==' ') {
											s->outpos--;
										}
										strcpy(&s->outbuf[s->outpos], "//-->");
										s->outpos += 5;
									}
									s->inbuf[s->inpos] = saveinchar;
									strcpy(&s->outbuf[s->outpos], endscripttag);
									s->outpos += 8;
								}
							} else {
								s->outbuf[s->outpos] = 0;
								s->outpos -= 7;
								warning(s, "%s(%d,%d): WARNING: '</script>' found in string or comment (ignored).\n", filename, s->linenum, 0);
								s->outpos += 7;
								warning(s, "%s%c...\n", s->outbuf, 0, (char) c);
							}
						}
					} else if (s->flags&SPACESTRIP) {
						s->flags &= ~SPACEHANDLING;
						s->flags |= SPACEASIS;
					}
					writechar(s, c, fout);
					break;
				}
				case '\"':
				case '\'':
				case '`': {
					if (!(s->flags&BACKSLASH)) {
						if (strchr(ISCOMMENT, s->quoted)) {
							if (!s->commentquoted) {
								s->commentquoted = (char) c;
							} else if (c==s->commentquoted) {
								s->commentquoted = 0;
							}
						}
					}
					if (s->flags&SPACESTRIP) {
						s->flags &= ~SPACEHANDLING;
						s->flags |= SPACEASIS;
					}
					writechar(s, c, fout);
					break;
				}
				default: {
					if (s->flags&SPACESTRIP) {
						s->flags &= ~SPACEHANDLING;
						s->flags |= SPACEASIS;
					}
					writechar(s, c, fout);
					break;
				}
			}
			if (s->flags&BACKSLASH) {
				s->flags &= ~BACKSLASH;
			} else if (strchr(ISCCOMMENT, s->quoted)) {
				if (c==s->quoted) {
					s->flags |= ALMOSTEND;
				} else if (s->flags&ALMOSTEND) {
					if (c=='/') {
						s->quoted = 0;
						s->numstrip = 0;
						s->flags &= ~SPACEHANDLING;
						if (s->flags&STRIPMODE) {
							if (!s->outpos || strchr(XXXX15, s->outbuf[s->outpos-1])) {
								s->flags |= SPACESTRIP;
							} else {
								s->flags |= SPACENEEDED;
							}
						} else {
							s->flags |= SPACEASIS;
						}
					}
					s->flags &= ~ALMOSTEND;
				}
			} else if (c=='\\') {
				if (s->quoted!='`') {
					s->flags |= BACKSLASH;
				}
			} else if ((s->quoted=='%') && (c=='?'||c=='#')) {
				s->quoted = 0;
				s->numstrip = 0;
				cmdtype = (char) c;
				s->flags &= ~(SPACEHANDLING|BACKSLASH);
				s->flags |= SPACEASIS;
			} else if (s->quoted==c) {
				if (c=='<') {
					s->quoted = '%';
				} else {
					s->quoted = 0;
					s->numstrip = 0;
					s->flags &= ~(SPACEHANDLING|BACKSLASH);
					if (c=='\n') {
						s->flags |= SPACESTRIP;
					} else {
						s->flags |= SPACEASIS;
						if (c=='%') {
							c = fgetc(fin);
							if (c=='\r') {
								/* If it is CR then read one character ahead. */
								savechar = fgetc(fin);
								c = '\n';
							} else if (c=='@') {
								s->quoted = '<';
								continue;
							} else if (c=='=') {
								s->inbuf[s->inpos++] = (char) c;
								writechar(s, c, fout);
								c = fgetc(fin);
								if (c=='\r') {
									/* If it is CR then read one character ahead. */
									savechar = fgetc(fin);
									c = '\n';
								}
							}
							cmdtype = (char) '%';
							continue;
						}
					}
				}
			}
		} else {
			switch (c) {
				case '\'':
				case '\"':
				case '`': {
					char newquoted;
					newquoted = (char) c;
					if (s->flags&SPACENEEDED) {
						writechar(s, ' ', fout);
					} else if (s->flags&SPACENEEDLF) {
						writechar(s, '\n', fout);
					}
					if (!(s->flags&STRIPMODE) || (newquoted!='\n')) {
						writechar(s, c, fout);
					}
					s->quoted = newquoted;
					s->flags &= ~SPACEHANDLING;
					s->flags |= SPACEASIS;
					break;
				}
				case ':': {
					if (s->indent && s->indentstack[s->indent-1] == 'E') {
						if (s->flags&SPACENEEDED) {
							writechar(s, ' ', fout);
						} else if (s->flags&SPACENEEDLF) {
							writechar(s, '\n', fout);
						}
						writechar(s, ':', fout);
						s->flags &= ~SPACEHANDLING;
						s->flags |= SPACEASIS;
						break;
					}
					s->flags &= ~EXTRAINDENT;
					c = fgetc(fin);
					if (c=='\r') {
						/* If it is CR then read one character ahead. */
						savechar = fgetc(fin);
						c = '\n';
					} else if (c==':') {
						/* If it is ':' then we found a namespace "::" operator. */
						s->inbuf[s->inpos++] = (char) c;
						writechar(s, c, fout);
						writechar(s, c, fout);
						s->flags &= ~SPACEHANDLING;
						s->flags |= SPACEASIS;
						break;
					}
					while (s->indent && s->indentstack[s->indent-1]==';') {
						s->indent--;
					}
					if (s->indent && (s->indentstack[s->indent-1]==':')) {
						/* This colon is part of a ?: construct */
						s->indentstack[s->indent-1] = ';';
						s->flags &= ~SPACEHANDLING;
						if (s->flags&STRIPMODE) {
							s->flags |= SPACESTRIP;
						} else if (s->indent<=s->curindent) {
							s->flags |= SPACENEEDLF;
						} else {
							s->flags |= SPACENEEDED;
						}
					} else {
						/* This colon is not part of a ?: construct, so it might be part of
						 * a switch statement or a label */
						if ((s->indent==s->curindent) && !(s->flags&STRIPMODE)) {
							int islabel;
							int pos = s->outpos-3;
							while ((pos>=0) && memcmp(&s->outbuf[pos], "case", 4)) --pos;
							if (pos>0) {
								islabel = strchr(BEFORETOKEN, s->outbuf[pos-1])!=0;
							} else {
								islabel = pos>=0;
							}
							if (islabel) {
								/* This is a case statement */
								s->flags |= EXTRAINDENT;
							} else if (checkkey(s, "default")) {
								islabel = 1;
								s->flags |= EXTRAINDENT;
							} else {
								pos = s->outpos-1;
								while (pos>=0 && !strchr(BEFORETOKEN, s->outbuf[pos])) {
									--pos;
								}
								if (pos!=s->outpos-1) {
									while (pos>=0 && strchr(SPACES, s->outbuf[pos])) {
										--pos;
									}
									islabel = pos<0;
								}
							}
							if (islabel) {
								if ((s->flags&EXTRAINDENT) && (!s->indent || s->indentstack[s->indent-1]!=' ')) {
									s->indentpos[s->indent] = s->outpos;
									s->indentstack[s->indent++] = ' ';
									s->indentflags[s->indent] = s->indentflags[s->indent-1];
								} else {
									int backsndent = (s->tabs>=0)? s->tabs: 1;
									if (backsndent && s->outpos>=backsndent) {
										s->outpos -= backsndent;
										memmove(s->outbuf, &s->outbuf[backsndent], s->outpos);
									}
								}
							}
						}
						s->flags &= ~SPACEHANDLING;
						if (s->flags&STRIPMODE) {
							s->flags |= SPACESTRIP;
						} else if ((s->inpos>1) && strchr(XXXX16, s->inbuf[s->inpos-2])) {
							s->flags |= SPACEASIS;
						} else {
							s->flags |= SPACENEEDED;
						}
					}
					writechar(s, ':', fout);
					continue;
				}
				case ';':
				case ',': {
					int newindent = s->indent;
					if (c == ';' && newindent && s->indentstack[newindent-1]=='E') {
						newindent--;
					} else {
						while (newindent && strchr(XXXX12, s->indentstack[newindent-1])) {
							newindent--;
						}
					}
					s->flags &= ~(SPACEHANDLING|EXTRAINDENT);
					if (s->flags&STRIPMODE) {
						s->flags |= SPACESTRIP;
					} else if ((s->inpos>1) && strchr(XXXX16, s->inbuf[s->inpos-2])) {
						s->flags |= SPACEASIS;
					} else {
						s->flags |= SPACENEEDED;
					}
					writechar(s, c, fout);
					s->indent = newindent;
					break;
				}
				case '=': {
					if (s->flags&STRIPMODE) {
						s->flags &= ~SPACEHANDLING;
						s->flags |= SPACESTRIP;
					} else if ((s->inpos>1) && strchr(XXXX16, s->inbuf[s->inpos-2])) {
						s->flags &= ~SPACEHANDLING;
						s->flags |= SPACEASIS;
					} else {
						int i;
						int spaceinsert;
						if (s->flags&SPACENEEDED) {
							writechar(s, ' ', fout);
						} else if (s->flags&SPACENEEDLF) {
							writechar(s, '\n', fout);
						}
						s->flags &= ~SPACEHANDLING;
						i = s->outpos-1;
						while ((i>=0) && strchr(XXXX17, s->outbuf[i])) i--;
						spaceinsert = i+1;
						while ((i>=0) && strchr(SPACES, s->outbuf[i])) {
							--i;
							spaceinsert = 0;
						}
						if ((i>6) && !memcmp(&s->outbuf[i-7], "operator", 8)) {
							s->flags |= SPACEASIS;
						} else {
							writechar(s, '=', fout);
							c = fgetc(fin);
							if (c=='\r') {
								/* If it is CR then read one character ahead. */
								savechar = fgetc(fin);
								c = '\n';
							}
							if (strchr("=>", c)) {
								/* Special cases "==" and "=>" (php) operators. */
								s->flags |= SPACEASIS;
							} else {
								/* Make sure there is a space before next character. */
								if (spaceinsert) {
									memmove(s->outbuf+spaceinsert+1, s->outbuf+spaceinsert, s->outpos-spaceinsert);
									s->outbuf[spaceinsert] = ' ';
									s->outpos++;
								}
								s->flags |= SPACENEEDED;
							}
							continue;
						}
					}
					writechar(s, c, fout);
					break;
				}
				case '?': {
					s->indentpos[s->indent] = s->outpos;
					s->flags &= ~SPACEHANDLING;
					if (s->flags&STRIPMODE) {
						s->flags |= SPACESTRIP;
					} else if ((s->inpos>1) && strchr(XXXX16, s->inbuf[s->inpos-2])) {
						s->flags |= SPACEASIS;
					} else {
						s->flags |= SPACENEEDED;
					}
					writechar(s, c, fout);
					s->indentstack[s->indent++] = ':';
					s->indentflags[s->indent] = s->indentflags[s->indent-1];
					break;
				}
				case '(': {
					char indenttype = ')';
					if (checkkey(s, "elseif")) {
						indenttype = '(';
					} else if (checkkey(s, "if") || checkkey(s, "for") || checkkey(s, "while")) {
						if (s->indent && s->indentstack[s->indent-1]==';') --s->indent;
						indenttype = '(';
					}
					if (!(s->flags&STRIPMODE)) {
						if (s->flags&SPACENEEDED) {
							writechar(s, ' ', fout);
						} else if (s->flags&SPACENEEDLF) {
							writechar(s, '\n', fout);
						}
					}
					s->indentpos[s->indent] = s->outpos;
					s->flags &= ~SPACEHANDLING;
					s->flags |= SPACESTRIP;
					writechar(s, c, fout);
					s->indentstack[s->indent++] = indenttype;
					s->indentflags[s->indent] = s->indentflags[s->indent-1];
					break;
				}
				case '{': {
					if (s->flags&EXTRAINDENT) {
						if (s->indent && strchr(XXXX13, s->indentstack[s->indent-1])) --s->indent;
						s->flags &= ~EXTRAINDENT;
					}
					if (!(s->flags&STRIPMODE)) {
						if (s->flags&SPACENEEDLF) {
							writechar(s, '\n', fout);
						} else if ((s->flags&SPACENEEDED) || (s->outpos && (!strchr(XXXX18, s->outbuf[s->outpos-1])))) {
							writechar(s, ' ', fout);
						}
					}
					s->indentpos[s->indent] = s->outpos;
					s->flags &= ~SPACEHANDLING;
					s->flags |= SPACESTRIP;
					writechar(s, c, fout);
					s->indentstack[s->indent++] = '}';
					s->indentflags[s->indent] = s->indentflags[s->indent-1];
					break;
				}
				case '[': {
					if (!(s->flags&STRIPMODE) && ((s->inpos<8) || !memcmp(&s->inbuf[s->inpos-8], "delete", 6))) {
						if (s->flags&SPACENEEDED) {
							writechar(s, ' ', fout);
						} else if (s->flags&SPACENEEDLF) {
							writechar(s, '\n', fout);
						}
					}
					s->indentpos[s->indent] = s->outpos;
					s->flags &= ~SPACEHANDLING;
					s->flags |= SPACESTRIP;
					writechar(s, c, fout);
					s->indentstack[s->indent++] = ']';
					s->indentflags[s->indent] = s->indentflags[s->indent-1];
					break;
				}
				case ')':
				case '}':
				case ']': {
					int newindent;
					while (s->indent && strchr(XXXX11, s->indentstack[s->indent-1])) {
						s->indent--;
					}
					newindent = s->indent;
					if ((newindent>0) && (c==')') && (s->indentstack[newindent-1]=='(')) {
						if (!(s->flags&STRIPMODE) && (s->indent<s->curindent) && s->outpos) {
							writechar(s, '\n', fout);
						}
						s->indentstack[--s->indent] = ';';
						s->flags |= EXTRAINDENT;
					} else if ((newindent>0) && (c==s->indentstack[newindent-1])) {
						if (!(s->flags&STRIPMODE) && (s->indent<s->curindent) && s->outpos) {
							writechar(s, '\n', fout);
						}
						newindent = --s->indent;
					} else if (!(s->defaultflags&QUIETMODE)) {
						s->outbuf[s->outpos] = 0;
						warning(s, "%s(%d,%d): WARNING: Matching opening brace for '%c' not found.\n", filename, s->linenum, (char) c);
						warning(s, "%s%c...\n", s->outbuf, 0, (char) c);
					}
					s->flags &= ~SPACEHANDLING;
					if (s->flags&STRIPMODE) {
						s->flags |= SPACESTRIP;
					} else {
						s->flags |= SPACEASIS;
					}
					writechar(s, c, fout);
					s->indent = newindent;
					break;
				}
				case ' ':
				case '\t':
				case '\n': {
					if (checkkey(s, "EXEC")) {
						s->indentstack[s->indent++] = 'E';
					} else if (checkkey(s, "else") || checkkey(s, "do")) {
						if (!(s->flags&EXTRAINDENT)) {
							s->indentstack[s->indent++] = ';';
						}
						s->flags |= EXTRAINDENT;
					} else if (checkkey(s, "done") && s->indent && s->indentstack[s->indent-1]==';') {
						int backsndent = (s->tabs>=0)? s->tabs: 1;
						s->indent--;
						s->flags &= ~EXTRAINDENT;
						if (backsndent && !(s->flags&STRIPMODE)) {
							s->outpos -= backsndent;
							memmove(s->outbuf, &s->outbuf[backsndent], s->outpos);
						}
					}
					if (c=='\n') {
						s->flags &= ~SPACEHANDLING;
						s->flags |= SPACESTRIP;
						writechar(s, c, fout);
					} else if (!(s->flags&(SPACESTRIP|SPACENEEDLF))) {
						s->flags &= ~SPACEHANDLING;
						s->flags |= SPACENEEDED;
					}
					break;
				}
				case '/': {
					char nextquoted = s->quoted;
					char prevchar = (char) (s->outpos? s->outbuf[s->outpos-1]: (char) 0);
					if (s->flags&SPACENEEDED) {
						writechar(s, ' ', fout);
					} else if (s->flags&SPACENEEDLF) {
						writechar(s, '\n', fout);
					}
					c = fgetc(fin);
					if (c=='\r') {
						/* If it is CR then read one character ahead. */
						savechar = fgetc(fin);
						c = '\n';
					}
					if (c=='/') {
						nextquoted = '\n';
						s->commentquoted = 0;
					} else if (c=='*' || c=='+') {
						s->inbuf[s->inpos++] = (char) c;
						nextquoted = (char) c;
						s->commentquoted = 0;
						if (s->flags&STRIPMODE) {
							s->flags &= ~SPACEHANDLING;
							s->flags |= s->outpos? SPACESTRIP: SPACENEEDED;
							s->numstrip = 0;
						} else {
							writechar(s, '/', fout);
							s->numstrip = (s->tabs>=0)? s->indent * s->tabs: s->indent;
							s->numstrip += s->inpos-s->outpos-1;
							writechar(s, c, fout);
						}
						s->quoted = nextquoted;
						break;
					} else if (strchr(XXXX14, prevchar)) {
						nextquoted = '/';
						s->commentquoted = 0;
					}
					s->flags &= ~SPACEHANDLING;
					s->flags |= SPACEASIS;
					if (!(nextquoted && (s->flags&STRIPMODE))) {
						writechar(s, '/', fout);
					}
					s->quoted = nextquoted;
					continue;
				}
				case '#': {
					if (s->flags&(SPACENEEDED|SPACENEEDLF)) {
						writechar(s, ' ', fout);
					}
					if (!s->outpos) {
						s->quoted = '\n';
						s->commentquoted = 0;
					}
					s->flags &= ~SPACEHANDLING;
					s->flags |= SPACEASIS;
					writechar(s, c, fout);
					break;
				}
				case '>': {
					/* Check for occurrence of "%>", "#>", "?>", "</script>" or "/>" */
					if (s->flags&XMLMODE) {
						if (cmdtype) {
							if ((s->inpos>=2) && (s->inbuf[s->inpos-2]==cmdtype)) {
								s->quoted = '<';
								if (s->indent && s->indentstack[s->indent-1]==':') {
									s->indent--;
								}
								/* remove any spaces before %>, #> or ?> */
								while ((s->outpos>1) && ((s->outbuf[s->outpos-2]==' ') || (s->outbuf[s->outpos-2]=='\t'))) {
									--s->outpos;
								}
								s->outbuf[s->outpos-1] = cmdtype;
								s->flags &= ~SPACEHANDLING;
								s->flags |= SPACEASIS;
							}
						} else {
							if ((s->inpos>=2) && (s->inbuf[s->inpos-2]=='/') && ((s->inpos<3) || (s->inbuf[s->inpos-3]!='*'))) {
								s->quoted = '<';
								/* remove any spaces before '/' */
								while ((s->outpos>1) && ((s->outbuf[s->outpos-2]==' ') || (s->outbuf[s->outpos-2]=='\t'))) {
									--s->outpos;
								}
								s->outbuf[s->outpos-1] = '/';
								s->flags &= ~SPACEHANDLING;
								s->flags |= SPACEASIS;
							} else if ((s->outpos>=8) && !(s->flags&SPACENEEDED) && !memcmp(&s->outbuf[s->outpos-8], endscripttag, 8)) {
								s->quoted = '<';
								/* remove any spaces before </script */
								while ((s->outpos>8) && ((s->outbuf[s->outpos-9]==' ') || (s->outbuf[s->outpos-9]=='\t'))) {
									--s->outpos;
								}
								memcpy(&s->outbuf[s->outpos-8], endscripttag, 8);
								s->flags &= ~SPACEHANDLING;
								s->flags |= SPACEASIS;
							}
						}
					}
					/* no break here because further handling is equal to the default situation */
				}
				default: {
					s->flags &= ~EXTRAINDENT;
					if (s->flags&SPACENEEDED) {
						writechar(s, ' ', fout);
					} else if (s->flags&SPACENEEDLF) {
						writechar(s, '\n', fout);
					}
					s->flags &= ~SPACEHANDLING;
					s->flags |= SPACEASIS;
					writechar(s, c, fout);
					break;
				}

			}
		}
		/* read one char from input stream. Use the read ahead character if available. */
		if (savechar!='\n') {
			c = savechar;
			savechar = '\n';
		} else {
			c = fgetc(fin);
		}
		if (c=='\r') {
			/* If it is CR then read one character ahead. */
			savechar = fgetc(fin);
			c = '\n';
		}
	}
	if (s->outpos) {
		writechar(s, '\n', fout);
	}
	if (s->flags&ZIPMODE) {
		long int pos;
		s->flags |= CHANGED;
		if (!(s->defaultflags&(UNIXLFMODE|MACCRMODE))) {
			/* re-open file in binary mode */
			fclose(fin);
			fin = fopen(dest, "ab");
			if (!fin) {
				fclose(fout);
				return warning(s, "%s: ERROR: cannot re-open in binary mode for writing remaining after <CTRL>-Z.\n", dest, 0, 0);
			}
		} else {
			fflush(fout);
		}
		/* prepare ZIP trailer */
		pos = ftell(fout);
		memcpy(s->outbuf, "\032\120\113\005\006", 5);
		memset(&s->outbuf[5], 0, 18);
		s->outbuf[17] = (char) ++pos;
		s->outbuf[18] = (char) (pos>>8);
		s->outbuf[19] = (char) (pos>>16);
		s->outbuf[20] = (char) (pos>>24);
		/* write ZIP trailer */
		if (fout) {
			fwrite(s->outbuf, 1, 23, fout);
		}
	}
	if (fout) {
		fclose(fout);
	}
	fclose(fin);
	if (s->quoted=='*') {
		warning(s, "%s(%d,%d): ERROR: */ missing at end of file.\n", filename, s->linenum, 0);
		if (!(s->defaultflags&CHANGED)) {
			s->flags &= ~CHANGED;
			return (s->flags&NOTESTMODE)? warning(s, ERRORMESSAGE, dest, 0, 0): s->flags;
		}
	} else if ((s->quoted=='\'') || (s->quoted=='\"') || (s->quoted=='/')) {
		warning(s, "%s(%d,%d): ERROR: %c missing at end of file.\n", filename, s->linenum, s->quoted);
		if (!(s->defaultflags&CHANGED)) {
			s->flags &= ~CHANGED;
			return (s->flags&NOTESTMODE)? warning(s, ERRORMESSAGE, dest, 0, 0): s->flags;
		}
	} else if ((s->flags&XMLMODE) && (s->quoted!='<')) {
		if (cmdtype) {
			warning(s, "%s(%d,%d): ERROR: %c> missing at end of file.\n", filename, s->linenum, cmdtype);
		} else {
			warning(s, "%s(%d,%d): ERROR: </script> missing at end of file.\n", filename, s->linenum, 0);
		}
		if (!(s->defaultflags&CHANGED)) {
			s->flags &= ~CHANGED;
			if (s->flags&NOTESTMODE) {
				warning(s, ERRORMESSAGE, dest, 0, 0);
			}
		}
		return s->flags;
	} else {
		while (s->indent && strchr(XXXX11, s->indentstack[s->indent-1])) s->indent--;
		if (s->indent--) {
			if (s->indentstack[s->indent]=='(') {
				s->indentstack[s->indent] = ')';
			}
			warning(s, "%s(%d,%d): ERROR: %c", filename, s->linenum, s->indentstack[s->indent]);
			while (s->indent--) {
				if (!strchr(XXXX11, s->indentstack[s->indent])) {
					if (s->indentstack[s->indent]=='(') {
						s->indentstack[s->indent] = ')';
					}
					warning(s, "%s%c", ", ", 0, s->indentstack[s->indent]);
				}
			}
			warning(s, " missing at end of file.\n", filename, 0, 0);
			if (!(s->defaultflags&CHANGED)) {
				s->flags &= ~CHANGED;
				if (s->flags&NOTESTMODE) {
					warning(s, ERRORMESSAGE, dest, 0, 0);
				}
			}
			return s->flags;
		}
	}
	if (s->outdir || s->flags&STRIPMODE) {
		s->flags |= CHANGED;
		if ((s->defaultflags&NOTESTMODE) && !(s->defaultflags&QUIETMODE)) {
			if (s->flags&XMLMODE) {
				return warning(s, "%s written (xml/html/sgml mode)\n", dest, 0, 0);
			} else if (s->flags& ZIPMODE) {
				return warning(s, "%s written (trailer contains empty ZIP-file)\n", dest, 0, 0);
			} else {
				return warning(s, "%s written\n", dest, 0, 0);
			}
		}
	} else if (!(s->defaultflags&NOTESTMODE)) {
		if (s->flags&CHANGED && !(s->defaultflags & QUIETMODE)) {
			return warning(s, "%s not written (test mode)\n", dest, 0, 0);
		}
		return s->flags;
	} else if (s->flags&CHANGED) {
		fin = fopen(filename, "rb");
		if (!fin) {
			warning(s, "%s: ERROR: cannot open\n", filename, 0, 0);
		} else {
			fout = fopen(orig, "wb");
			if (!fout) {
				fclose(fin);
				warning(s, "%s: ERROR: cannot open\n", filename, 0, 0);
			} else {
				s->outpos = (int) fread(s->outbuf, 1, sizeof(s->outbuf), fin);
				while (s->outpos==sizeof(s->outbuf)) {
					fwrite(s->outbuf, 1, sizeof(s->outbuf), fout);
					s->outpos = (int) fread(s->outbuf, 1, sizeof(s->outbuf), fin);
				}
				if (s->outpos>0) {
					fwrite(s->outbuf, 1, s->outpos, fout);
				}
				fclose(fout);
				fclose(fin);

				fin = fopen(dest, "rb");
				if (!fin) {
					warning(s, "%s: ERROR: cannot open\n", dest, 0, 0);
				} else {
					fout = fopen(filename, "wb");
					if (!fout) {
						warning(s, "%s: ERROR: should be writable\n", filename, 0, 0);
						removefile(s, orig);
					} else {
						s->outpos = (int) fread(s->outbuf, 1, sizeof(s->outbuf), fin);
						while (s->outpos==sizeof(s->outbuf)) {
							fwrite(s->outbuf, 1, sizeof(s->outbuf), fout);
							s->outpos = (int) fread(s->outbuf, 1, sizeof(s->outbuf), fin);
						}
						if (s->outpos>0) {
							fwrite(s->outbuf, 1, s->outpos, fout);
						}
						fclose(fout);
						removefile(s, dest);
						if (!(s->defaultflags&QUIETMODE)) {
							if (s->flags&XMLMODE) {
								warning(s, "%s modified (xml/html/sgml mode)\n", filename, 0, 0);
							} else if (s->flags&ZIPMODE) {
								warning(s, "%s modified (trailer contains empty ZIP-file)\n", filename, 0, 0);
							} else {
								warning(s, "%s modified\n", filename, 0, 0);
							}
						}
					}
					fclose(fin);
				}
			}
		}
	} else {
		removefile(s, dest);
	}
	return s->flags;
}

/**
 * print usage instructions to output stream.
 *
 * @param s BUSL status
 * @param argv0 first command line argument
 */
int __stdcall busl_usage(Busl *s, const char *argv0) {
	const char *p = &argv0[strlen(argv0)];
	while ((p>argv0) && !strchr(DIRSEPARATOR, p[-1])) --p;
	warning(s, "usage:\t%s <options> [<output-dir>] <files>\n", p, 0, 0);
	warning(s, "\t0 no indenting\n", COPYRIGHT, 0, 0);
	warning(s, "\t4 indenting 4 spaces/level\n", COPYRIGHT, 0, 0);
	warning(s, "\t-4 indenting 1 tab=4 spaces/level (default)\n", COPYRIGHT, 0, 0);
	warning(s, "\ta automatic detection of mode (default)\n", COPYRIGHT, 0, 0);
	warning(s, "\tf force output\n", COPYRIGHT, 0, 0);
	warning(s, "\tg generic mode (default) (resets a, x)\n", COPYRIGHT, 0, 0);
	warning(s, "\tl linefeed mode\n", COPYRIGHT, 0, 0);
	warning(s, "\tq quiet mode\n", COPYRIGHT, 0, 0);
	warning(s, "\tr carriage return mode\n", COPYRIGHT, 0, 0);
	warning(s, "\ts strip mode\n", COPYRIGHT, 0, 0);
	warning(s, "\tt test mode\n", COPYRIGHT, 0, 0);
	warning(s, "\tx xml/html/sgml mode (resets a, g)\n", COPYRIGHT, 0, 0);
	warning(s, "\tz prepare as zip file (not usable with x)\n", COPYRIGHT, 0, 0);
	warning(s, "\t@<file> read command line options from file\n", COPYRIGHT, 0, 0);
	warning(s, "\t<output-dir> should end with '/' or '\\' (default './')\n", COPYRIGHT, 0, 0);
	return CHANGED;
}

/**
 * clean-up BUSL status.
 *
 * @param s BUSL status
 * @param changed
 */
int __stdcall busl_finish(Busl *s, int changed) {
	if (!(changed&CHANGED || (s->defaultflags&QUIETMODE))) {
		warning(s, "no sources modified\n", COPYRIGHT, 0, 0);
	}
	if (s->result < 0) {
		s->result = EXIT_SUCCESS;
	}
	while (s->first) {
		char *old = (char *) s->first;
		if (unlink(s->first->name)) {
			usleep(200000);
			if (unlink(s->first->name)) {
				warning(s, "%s: WARNING: cannot be removed\n", s->first->name, 0, 0);
			}
		}
		s->first = s->first->next;
		free(old);
	}
	return s->result;
}

/**
 * constructor.
 */
Busl *__stdcall busl_create(Busl *s, void (__stdcall* wrt)(void *, const char *), void *output) {
	if (!s) {
		s = (Busl *) malloc(sizeof(Busl));
	}
	memset(s, 0, sizeof(Busl));
	s->wrt = wrt;
	s->output = output? output: (void *) s;
	s->defaultflags = AUTOMODE|NOTESTMODE;
	s->result = -1;
	s->tabs = -4;
	return s;
}

/**
 * destructor.
 *
 * @param s BUSL status
 */
void __stdcall busl_delete(Busl *s) {
	free((char *) s);
}
