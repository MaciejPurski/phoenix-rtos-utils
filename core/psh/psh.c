/*
 * Phoenix-RTOS
 *
 * Phoenix-RTOS SHell
 *
 * Copyright 2017, 2018, 2020 Phoenix Systems
 * Author: Pawel Pisarczyk, Jan Sikorski, Lukasz Kosinski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#include <posix/utils.h>

#include "psh.h"


/* Shell definitions */
#define PROMPT       "(psh)% "     /* Shell prompt */
#define SCRIPT_MAGIC ":{}:"        /* Every psh script should start with this line */
#define HISTSZ       512           /* Command history size */


/* Special key codes */
#define UP           "[A"          /* Up */
#define DOWN         "[B"          /* Down */
#define RIGHT        "[C"          /* Right */
#define LEFT         "[D"          /* Left */
#define DELETE       "[3~"         /* Delete */


/* Misc definitions */
#define BP_OFFS      0             /* Offset of 0 exponent entry in binary prefix table */
#define BP_EXP_OFFS  10            /* Offset between consecutive entries exponents in binary prefix table */
#define SI_OFFS      8             /* Offset of 0 exponent entry in SI prefix table */
#define SI_EXP_OFFS  3             /* Offset between consecutive entries exponents in SI prefix table */


typedef struct {
	int n;                         /* Command length (each word is followed by '\0') */
	char *cmd;                     /* Command pointer */
} psh_histent_t;


typedef struct {
	int hb;                        /* History begin index (oldest command) */
	int he;                        /* History end index (newest command) */
	psh_histent_t entries[HISTSZ]; /* Command history entries */
} psh_hist_t;


/* Shell commands */
extern int psh_bind(int argc, char **argv);
extern int psh_cat(int argc, char **argv);
extern int psh_kill(int argc, char **argv);
extern int psh_ls(int argc, char **argv);
extern int psh_mem(int argc, char **argv);
extern int psh_mkdir(int argc, char **argv);
extern int psh_mount(int argc, char **argv);
extern int psh_perf(int argc, char **argv);
extern int psh_ps(int argc, char **argv);
extern int psh_reboot(int argc, char **argv);
extern int psh_sync(int argc, char **argv);
extern int psh_top(int argc, char **argv);
extern int psh_touch(int argc, char **argv);


/* Binary (base 2) prefixes */
static const char *bp[] = {
	"",   /* 2^0       */
	"K",  /* 2^10   kibi */
	"M",  /* 2^20   mebi */
	"G",  /* 2^30   gibi */
	"T",  /* 2^40   tebi */
	"P",  /* 2^50   pebi */
	"E",  /* 2^60   exbi */
	"Z",  /* 2^70   zebi */
	"Y"   /* 2^80   yobi */
};


/* SI (base 10) prefixes */
static const char* si[] = {
	"y",  /* 10^-24 yocto */
	"z",  /* 10^-21 zepto */
	"a",  /* 10^-18 atto  */
	"f",  /* 10^-15 femto */
	"p",  /* 10^-12 pico  */
	"n",  /* 10^-9  nano  */
	"u",  /* 10^-6  micro */
	"m",  /* 10^-3  milli */
	"",   /* 10^0         */
	"k",  /* 10^3   kilo  */
	"M",  /* 10^6   mega  */
	"G",  /* 10^9   giga  */
	"T",  /* 10^12  tera  */
	"P",  /* 10^15  peta  */
	"E",  /* 10^18  exa   */
	"Z",  /* 10^21  zetta */
	"Y",  /* 10^24  yotta */
};


psh_common_t psh_common;


static int psh_mod(int x, int y)
{
	int ret = x % y;

	if (ret < 0)
		ret += abs(y);

	return ret;
}


static int psh_div(int x, int y)
{
	return (x - psh_mod(x, y)) / y;
}


static int psh_log(unsigned int base, unsigned int x)
{
	int ret = 0;

	while (x /= base)
		ret++;

	return ret;
}


static int psh_pow(int x, unsigned int y)
{
	int ret = 1;

	while (y) {
		if (y & 1)
			ret *= x;
		y >>= 1;
		if (!y)
			break;
		x *= x;
	}

	return ret;
}


static const char *psh_bp(int exp)
{
	exp = psh_div(exp, BP_EXP_OFFS) + BP_OFFS;

	if ((exp < 0) || (exp >= sizeof(bp) / sizeof(bp[0])))
		return NULL;

	return bp[exp];
}


static const char *psh_si(int exp)
{
	exp = psh_div(exp, SI_EXP_OFFS) + SI_OFFS;

	if ((exp < 0) || (exp >= sizeof(si) / sizeof(si[0])))
		return NULL;

	return si[exp];
}


int psh_prefix(unsigned int base, int x, int y, unsigned int prec, char *buff)
{
	int div = psh_log(base, abs(x)), exp = div + y;
	int offs, ipart, fpart;
	const char *(*fp)(int);
	const char *prefix;
	char fmt[11];

	/* Support precision for up to 8 decimal places */
	if (prec > 8)
		return -EINVAL;

	switch (base) {
	/* Binary prefix */
	case 2:
		fp = psh_bp;
		offs = BP_EXP_OFFS;
		break;

	/* SI prefix */
	case 10:
		fp = psh_si;
		offs = SI_EXP_OFFS;
		break;

	default:
		return -EINVAL;
	}

	/* div < 0 => accumulate extra exponents in x */
	if ((div -= psh_mod(exp, offs)) < 0) {
		x *= psh_pow(base, -div);
		div = 0;
	}
	div = psh_pow(base, div);

	/* Save integer part and fractional part as percentage */
	ipart = abs(x) / div;
	fpart = (int)((uint64_t)psh_pow(10, prec + 1) * (abs(x) % div) / div);

	/* Round the result */
	if ((fpart = (fpart + 5) / 10) == psh_pow(10, prec)) {
		ipart++;
		fpart = 0;
		if (ipart == psh_pow(base, offs)) {
			ipart = 1;
			exp += offs;
		}
	}

	/* Remove trailing zeros */
	while (fpart && !(fpart % 10)) {
		fpart /= 10;
		prec--;
	}

	/* Get the prefix */
	if ((prefix = fp((!ipart && !fpart) ? y : exp)) == NULL)
		return -EINVAL;

	if (x < 0)
		*buff++ = '-';

	if (fpart) {
		sprintf(fmt, "%%d.%%0%ud%%s", prec);
		sprintf(buff, fmt, ipart, fpart, prefix);
	}
	else {
		sprintf(buff, "%d%s", ipart, prefix);
	}

	return EOK;
}


static int psh_extendcmd(char **cmd, int *cmdsz, int n)
{
	char *rcmd;

	if ((rcmd = (char *)realloc(*cmd, n)) == NULL) {
		printf("psh: out of memory\r\n");
		free(*cmd);
		return -ENOMEM;
	}
	*cmd = rcmd;
	*cmdsz = n;

	return EOK;
}


static int psh_histentcmd(char **cmd, int *cmdsz, psh_histent_t *entry, int n)
{
	int err, i;

	if ((*cmdsz < n) && ((err = psh_extendcmd(cmd, cmdsz, n)) < 0))
		return err;

	for (i = 0; i < entry->n; i++)
		(*cmd)[i] = (entry->cmd[i] == '\0') ? ' ' : entry->cmd[i];

	return entry->n;
}


static void psh_printhistent(psh_histent_t *entry)
{
	int i;

	write(STDOUT_FILENO, "\r\033[0J", 5);
	write(STDOUT_FILENO, PROMPT, sizeof(PROMPT) - 1);

	for (i = 0; i < entry->n; i++)
		write(STDOUT_FILENO, (entry->cmd[i] == '\0') ? " " : entry->cmd + i, 1);
}


static void psh_movecursor(int col, int n)
{
	struct winsize ws;
	char fmt[8];
	int p;

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) < 0) {
		ws.ws_row = 25;
		ws.ws_col = 80;
	}
	col %= ws.ws_col;

	if (col + n < 0) {
		p = (-(col + n) + ws.ws_col - 1) / ws.ws_col;
		n += p * ws.ws_col;
		sprintf(fmt, "\033[%dA", p);
		write(STDOUT_FILENO, fmt, strlen(fmt));
	}
	else if (col + n > ws.ws_col - 1) {
		p = (col + n) / ws.ws_col;
		n -= p * ws.ws_col;
		sprintf(fmt, "\033[%dB", p);
		write(STDOUT_FILENO, fmt, strlen(fmt));
	}

	if (n > 0) {
		sprintf(fmt, "\033[%dC", n);
		write(STDOUT_FILENO, fmt, strlen(fmt));
	}
	else if (n < 0) {
		sprintf(fmt, "\033[%dD", -n);
		write(STDOUT_FILENO, fmt, strlen(fmt));
	}
}


extern void cfmakeraw(struct termios *termios);


static int psh_readcmd(struct termios *orig, psh_hist_t *cmdhist, char **cmd)
{
	int err, esc, l, n, m, hn, hm, hp = cmdhist->he, cmdsz = 128;
	struct termios raw = *orig;
	char c, *escp, tmp[8];

	/* Enable raw mode for command processing */
	cfmakeraw(&raw);
	if ((err = tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw)) < 0) {
		printf("psh: failed to enable raw mode\n");
		return err;
	}

	if ((*cmd = (char *)malloc(cmdsz)) == NULL) {
		printf("psh: out of memory\n");
		return -ENOMEM;
	}

	for (n = 0, m = 0, hn = 0, hm = 0, esc = -1;;) {
		read(STDIN_FILENO, &c, 1);

		/* Process control characters */
		if ((c < 0x20) || (c == 0x7f)) {
			/* Print not recognized escape codes */
			if (esc != -1) {
				l = n - esc;
				if (hp != cmdhist->he) {
					memcpy(tmp, *cmd + esc, l);
					if ((err = psh_histentcmd(cmd, &cmdsz, cmdhist->entries + hp, cmdhist->entries[hp].n + l)) < 0)
						return err;
					hp = cmdhist->he;
					n = hn;
					m = hm;
					memmove(*cmd + n + l, *cmd + n, m);
					memcpy(*cmd + n, tmp, l);
					n += l;
				}
				write(STDOUT_FILENO, *cmd + n - l, l + m);
				psh_movecursor(n + m + sizeof(PROMPT) - 1, -m);
				esc = -1;
			}

			/* ETX => cancel command */
			if (c == '\003') {
				write(STDOUT_FILENO, "^C", 2);
				if (hp == cmdhist->he) {
					if (m > 2)
						psh_movecursor(n + sizeof(PROMPT) + 1, m - 2);
				}
				else {
					if (hm > 2)
						psh_movecursor(hn + sizeof(PROMPT) + 1, hm - 2);
				}
				write(STDOUT_FILENO, "\r\n", 2);
				n = m = 0;
				break;
			}
			/* EOT => delete next character/exit */
			else if (c == '\004') {
				if (hp != cmdhist->he) {
					if (hm) {
						if ((err = psh_histentcmd(cmd, &cmdsz, cmdhist->entries + hp, cmdhist->entries[hp].n + 2)) < 0)
							return err;
						hp = cmdhist->he;
						n = hn;
						m = hm;
					}
					else {
						continue;
					}
				}
				if (m) {
					memmove(*cmd + n, *cmd + n + 1, --m);
					write(STDOUT_FILENO, "\033[0J", 4);
					write(STDOUT_FILENO, *cmd + n, m);
					psh_movecursor(n + m + sizeof(PROMPT) - 1, -m);
				}
				else if (!(n + m)) {
					write(STDOUT_FILENO, "exit\r\n", 6);
					exit(EXIT_SUCCESS);
				}
			}
			/* BS => remove last character */
			else if (c == '\b') {
				if (hp != cmdhist->he) {
					if (hn) {
						if ((err = psh_histentcmd(cmd, &cmdsz, cmdhist->entries + hp, cmdhist->entries[hp].n + 2)) < 0)
							return err;
						hp = cmdhist->he;
						n = hn;
						m = hm;
					}
					else {
						continue;
					}
				}
				if (n) {
					write(STDOUT_FILENO, &c, 1);
					n--;
					memmove(*cmd + n, *cmd + n + 1, m);
					write(STDOUT_FILENO, "\033[0J", 4);
					write(STDOUT_FILENO, *cmd + n, m);
					psh_movecursor(n + m + sizeof(PROMPT) - 1, -m);
				}
			}
			/* TAB => autocomplete */
			else if (c == '\t') {
				
			}
			/* FF => clear screen */
			else if (c == '\014') {
				write(STDOUT_FILENO, "\033[2J", 4);
				write(STDOUT_FILENO, "\033[f", 3);
				if (hp != cmdhist->he) {
					psh_printhistent(cmdhist->entries + hp);
					psh_movecursor(cmdhist->entries[hp].n + sizeof(PROMPT) - 1, -hm);
				}
				else {
					write(STDOUT_FILENO, PROMPT, sizeof(PROMPT) - 1);
					write(STDOUT_FILENO, *cmd, n + m);
					psh_movecursor(n + m + sizeof(PROMPT) - 1, -m);
				}
			}
			/* LF or CR => go to new line and break (finished reading command) */
			else if ((c == '\r') || (c == '\n')) {
				if (hp != cmdhist->he) {
					if ((err = psh_histentcmd(cmd, &cmdsz, cmdhist->entries + hp, cmdhist->entries[hp].n + 2)) < 0)
						return err;
					hp = cmdhist->he;
					n = hn;
					m = hm;
				}
				psh_movecursor(n + sizeof(PROMPT) - 1, m);
				write(STDOUT_FILENO, "\r\n", 2);
				break;
			}
			/* ESC => process escape code keys */
			else if (c == '\033') {
				esc = n;
			}
		}
		/* Process regular characters */
		else {
			if ((n + m > cmdsz - 2) && ((err = psh_extendcmd(cmd, &cmdsz, 2 * cmdsz)) < 0))
				return err;

			if (esc == -1) {
				if (hp != cmdhist->he) {
					if ((err = psh_histentcmd(cmd, &cmdsz, cmdhist->entries + hp, cmdhist->entries[hp].n + 2)) < 0)
						return err;
					hp = cmdhist->he;
					n = hn;
					m = hm;
				}
				memmove(*cmd + n + 1, *cmd + n, m);
				(*cmd)[n++] = c;
				write(STDOUT_FILENO, *cmd + n - 1, m + 1);
				psh_movecursor(n + m + sizeof(PROMPT) - 1, -m);
			}
			else {
				memmove(*cmd + n + 1, *cmd + n, m);
				(*cmd)[n++] = c;
				escp = *cmd + esc;
				l = n - esc;

				if (!strncmp(escp, UP, l)) {
					if (l == sizeof(UP) - 1) {
						n -= sizeof(UP) - 1;
						memmove(*cmd + n, *cmd + n + sizeof(UP) - 1, m);
						esc = -1;
						if (hp != cmdhist->hb) {
							l = ((hp == cmdhist->he) ? n : hn) + sizeof(PROMPT) - 1;
							psh_movecursor(l, -l);
							psh_printhistent(cmdhist->entries + (hp = (hp) ? hp - 1 : HISTSZ - 1));
							hn = cmdhist->entries[hp].n;
							hm = 0;
						}
					}
				}
				else if (!strncmp(escp, DOWN, l)) {
					if (l == sizeof(DOWN) - 1) {
						n -= sizeof(DOWN) - 1;
						memmove(*cmd + n, *cmd + n + sizeof(DOWN) - 1, m);
						esc = -1;
						if (hp != cmdhist->he) {
							l = hn + sizeof(PROMPT) - 1;
							psh_movecursor(l, -l);
							if ((hp = (hp + 1) % HISTSZ) == cmdhist->he) {
								write(STDOUT_FILENO, "\r\033[0J", 5);
								write(STDOUT_FILENO, PROMPT, sizeof(PROMPT) - 1);
								n += m;
								m = 0;
								write(STDOUT_FILENO, *cmd, n);
							}
							else {
								psh_printhistent(cmdhist->entries + hp);
								hn = cmdhist->entries[hp].n;
								hm = 0;
							}
						}
					}
				}
				else if (!strncmp(escp, RIGHT, l)) {
					if (l == sizeof(RIGHT) - 1) {
						n -= sizeof(RIGHT) - 1;
						memmove(*cmd + n, *cmd + n + sizeof(RIGHT) - 1, m);
						esc = -1;
						if (hp == cmdhist->he) {
							if (m) {
								psh_movecursor(n + sizeof(PROMPT) - 1, 1);
								n++;
								m--;
							}
						}
						else {
							if (hm) {
								psh_movecursor(hn + sizeof(PROMPT) - 1, 1);
								hn++;
								hm--;
							}
						}
					}
				}
				else if (!strncmp(escp, LEFT, l)) {
					if (l == sizeof(LEFT) - 1) {
						n -= sizeof(LEFT) - 1;
						memmove(*cmd + n, *cmd + n + sizeof(LEFT) - 1, m);
						esc = -1;
						if (hp == cmdhist->he) {
							if (n) {
								psh_movecursor(n + sizeof(PROMPT) - 1, -1);
								n--;
								m++;
							}
						}
						else {
							if (hn) {
								psh_movecursor(hn + sizeof(PROMPT) - 1, -1);
								hn--;
								hm++;
							}
						}
					}
				}
				else if (!strncmp(escp, DELETE, l)) {
					if (l == sizeof(DELETE) - 1) {
						n -= sizeof(DELETE) - 1;
						memmove(*cmd + n, *cmd + n + sizeof(DELETE) - 1, m);
						esc = -1;
						if (hp != cmdhist->he) {
							if (hm) {
								if ((err = psh_histentcmd(cmd, &cmdsz, cmdhist->entries + hp, cmdhist->entries[hp].n + 2)) < 0)
									return err;
								hp = cmdhist->he;
								n = hn;
								m = hm;
							}
							else {
								continue;
							}
						}
						if (m) {
							memmove(*cmd + n, *cmd + n + 1, --m);
							write(STDOUT_FILENO, "\033[0J", 4);
							write(STDOUT_FILENO, *cmd + n, m);
							psh_movecursor(n + m + sizeof(PROMPT) - 1, -m);
						}
					}
				}
				else {
					if (hp != cmdhist->he) {
						memcpy(tmp, escp, l);
						if ((err = psh_histentcmd(cmd, &cmdsz, cmdhist->entries + hp, cmdhist->entries[hp].n + l)) < 0)
							return err;
						hp = cmdhist->he;
						n = hn;
						m = hm;
						memmove(*cmd + n + l, *cmd + n, m);
						memcpy(*cmd + n, tmp, l);
						n += l;
					}
					write(STDOUT_FILENO, *cmd + n - l, l + m);
					psh_movecursor(n + m + sizeof(PROMPT) - 1, -m);
					esc = -1;
				}
			}
		}
	}
	(*cmd)[n + m] = '\0';

	/* Restore original terminal settings */
	if ((err = tcsetattr(STDIN_FILENO, TCSAFLUSH, orig)) < 0) {
		printf("psh: failed to disable raw mode\r\n");
		free(*cmd);
		return err;
	}

	return n + m;
}


static int psh_parsecmd(char *line, int *argc, char ***argv)
{
	char *cmd, *arg, **rargv;

	if ((cmd = strtok(line, "\t ")) == NULL)
		return -EINVAL;

	if ((*argv = (char **)malloc(2 * sizeof(char *))) == NULL)
		return -ENOMEM;

	*argc = 0;
	(*argv)[(*argc)++] = cmd;

	while ((arg = strtok(NULL, "\t ")) != NULL) {
		if ((rargv = (char **)realloc(*argv, (*argc + 2) * sizeof(char *))) == NULL) {
			free(*argv);
			return -ENOMEM;
		}

		*argv = rargv;
		(*argv)[(*argc)++] = arg;
	}

	(*argv)[*argc] = NULL;

	return EOK;
}


static int psh_runfile(char **argv)
{
	volatile int err = EOK;
	pid_t pid;

	if ((pid = vfork()) < 0) {
		printf("psh: vfork failed\n");
		return pid;
	}
	else if (!pid) {
		/* Put process in its own process group */
		pid = getpid();
		if (setpgid(pid, pid) < 0) {
			printf("psh: failed to put %s process in its own process group\n", argv[0]);
			exit(EXIT_FAILURE);
		}

		/* Take terminal control */
		tcsetpgrp(STDIN_FILENO, pid);

		/* Execute the file */
		exit(err = execve(argv[0], argv, NULL));
	}

	switch (err) {
	case EOK:
		err = waitpid(pid, NULL, 0);
		break;

	case -ENOMEM:
		printf("psh: out of memory\n");
		break;
		
	case -EINVAL:
		printf("psh: invalid executable\n");
		break;

	default:
		printf("psh: exec failed with code %d\n", err);
	}

	/* Take back terminal control */
	tcsetpgrp(STDIN_FILENO, getpgid(getpid()));

	return err;
}


static int psh_runscript(char *path)
{
	char **argv = NULL, *line = NULL;
	int i, err, argc = 0;
	size_t n = 0;
	ssize_t ret;
	FILE *stream;
	pid_t pid;

	if ((stream = fopen(path, "r")) == NULL) {
		printf("psh: failed to open file %s\n", path);
		return -EINVAL;
	}

	if ((getline(&line, &n, stream) < sizeof(SCRIPT_MAGIC)) || strncmp(line, SCRIPT_MAGIC, sizeof(SCRIPT_MAGIC) - 1)) {
		printf("psh: %s is not a psh script\n", path);
		free(line);
		fclose(stream);
		return -EINVAL;
	}

	free(line);
	line = NULL;
	n = 0;

	for (i = 2; (ret = getline(&line, &n, stream)) > 0; i++) {
		if (line[0] == 'X' || line[0] == 'W') {
			if (line[ret - 1] == '\n')
				line[ret - 1] = '\0';

			do {
				if ((err = psh_parsecmd(line + 1, &argc, &argv)) < 0) {
					printf("psh: failed to parse line %d\n", i);
					break;
				}

				if ((pid = vfork()) < 0) {
					printf("psh: vfork failed in line %d\n", i);
					err = pid;
					break;
				}
				else if (!pid) {
					err = execve(argv[0], argv, NULL);
					printf("psh: exec failed in line %d\n", i);
					exit(EXIT_FAILURE);
				}

				if ((line[0] == 'W') && ((err = waitpid(pid, NULL, 0)) < 0)) {
					printf("psh: waitpid failed in line %d\n", i);
					break;
				}
			} while (0);

			free(argv);
			argv = NULL;
		}

		if (err < 0)
			break;

		free(line);
		line = NULL;
		n = 0;
	}

	free(line);
	fclose(stream);

	return EOK;
}


static int psh_exec(int argc, char **argv)
{
	int err;

	if (argc < 2) {
		printf("usage: %s command [args]...\n", argv[0]);
		return -EINVAL;
	}

	switch (err = execve(argv[1], argv + 1, NULL)) {
	case EOK:
		break;

	case -ENOMEM:
		printf("psh: out of memory\n");
		break;
		
	case -EINVAL:
		printf("psh: invalid executable\n");
		break;

	default:
		printf("psh: exec failed with code %d\n", err);
	}

	return err;
}


static void psh_help(void)
{
	printf("Available commands:\n");
	printf("  bind    - binds device to directory\n");
	printf("  cat     - concatenate file(s) to standard output\n");
	printf("  exec    - replace shell with the given command\n");
	printf("  exit    - exits the shell\n");
	printf("  help    - prints this help message\n");
	printf("  history - prints command history\n");
	printf("  kill    - terminates process\n");
	printf("  ls      - lists files in the namespace\n");
	printf("  mem     - prints memory map\n");
	printf("  mkdir   - creates directory\n");
	printf("  mount   - mounts a filesystem\n");
	printf("  perf    - tracks kernel performance\n");
	printf("  ps      - prints processes and threads\n");
	printf("  reboot  - restarts the machine\n");
	printf("  sync    - synchronizes device\n");
	printf("  top     - top utility\n");
	printf("  touch   - changes file timestamp\n");
}


static int psh_history(int argc, char **argv, psh_hist_t *cmdhist)
{
	psh_histent_t *entry;
	unsigned char clear = 0;
	int c, i, j, size;
	char fmt[12];

	while ((c = getopt(argc, argv, "ch")) != -1) {
		switch (c) {
		case 'c':
			clear = 1;
			break;

		case 'h':
		default:
			printf("usage: %s [options] or no args to print command history\n", argv[0]);
			printf("  -c:  clears command history\n");
			printf("  -h:  shows this help message\n");
			return EOK;
		}
	}

	if (clear) {
		for (i = cmdhist->hb; i != cmdhist->he; i = (i + 1) % HISTSZ)
			free(cmdhist->entries[i].cmd);
		cmdhist->hb = cmdhist->he = 0;
	}
	else {
		size = (cmdhist->hb < cmdhist->he) ? cmdhist->he - cmdhist->hb : HISTSZ - cmdhist->hb + cmdhist->he;
		sprintf(fmt, "  %%%du  ", psh_log(10, size) + 1);

		for (i = 0; i < size; i++) {
			entry = cmdhist->entries + (cmdhist->hb + i) % HISTSZ;
			printf(fmt, i + 1);
			for (j = 0; j < entry->n; j++)
				printf("%c", (entry->cmd[j] == '\0') ? ' ' : entry->cmd[j]);
			printf("\n");
		}
	}

	return EOK;
}


static void psh_signalint(int sig)
{
	psh_common.sigint = 1;
}


static void psh_signalquit(int sig)
{
	psh_common.sigquit = 1;
}


static void psh_signalstop(int sig)
{
	psh_common.sigstop = 1;
}


static int psh_run(void)
{
	psh_hist_t cmdhist = { .hb = 0, .he = 0 };
	psh_histent_t *entry;
	struct termios orig;
	char *cmd, **argv;
	int err, argc;
	pid_t pgrp;

	/* Check if we run interactively */
	if (!isatty(STDIN_FILENO))
		return -ENOTTY;

	/* Wait till we run in foreground */
	if (tcgetpgrp(STDIN_FILENO) != -1) {
		while ((pgrp = getpgrp()) != tcgetpgrp(STDIN_FILENO))
			kill(-pgrp, SIGTTIN);
	}

	/* Set signal handlers */
	signal(SIGINT, psh_signalint);
	signal(SIGQUIT, psh_signalquit);
	signal(SIGTSTP, psh_signalstop);
	signal(SIGTTIN, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);
	signal(SIGCHLD, SIG_IGN);

	/* Put ourselves in our own process group */
	pgrp = getpid();
	if ((err = setpgid(pgrp, pgrp)) < 0) {
		printf("psh: failed to put shell in its own process group\n");
		return err;
	}

	do {
		/* Save original terminal settings */
		if ((err = tcgetattr(STDIN_FILENO, &orig)) < 0) {
			printf("psh: failed to save terminal settings\n");
			break;
		}

		/* Take terminal control */
		if ((err = tcsetpgrp(STDIN_FILENO, pgrp)) < 0) {
			printf("psh: failed to take terminal control\n");
			break;
		}

		for (;;) {
			write(STDOUT_FILENO, "\r\033[0J", 5);
			write(STDOUT_FILENO, PROMPT, sizeof(PROMPT) - 1);

			if ((err = psh_readcmd(&orig, &cmdhist, &cmd)) < 0)
				break;

			if (psh_parsecmd(cmd, &argc, &argv) < 0) {
				free(cmd);
				continue;
			}

			/* Select command history entry */
			if (cmdhist.he != cmdhist.hb) {
				entry = &cmdhist.entries[(cmdhist.he) ? cmdhist.he - 1 : HISTSZ - 1];
				if ((err == entry->n) && !memcmp(cmd, entry->cmd, err)) {
					cmdhist.he = (cmdhist.he) ? cmdhist.he - 1 : HISTSZ - 1;
					free(entry->cmd);
				}
				else {
					entry = cmdhist.entries + cmdhist.he;
				}
			}
			else {
				entry = cmdhist.entries + cmdhist.he;
			}

			/* Update command history */
			entry->cmd = cmd;
			entry->n = err;
			if ((cmdhist.he = (cmdhist.he + 1) % HISTSZ) == cmdhist.hb) {
				free(cmdhist.entries[cmdhist.hb].cmd);
				cmdhist.hb = (cmdhist.hb + 1) % HISTSZ;
			}

			/* Clear signals */
			psh_common.sigint = 0;
			psh_common.sigquit = 0;
			psh_common.sigstop = 0;

			/* Reset getopt */
			optind = 1;

			if (!strcmp(argv[0], "bind"))
				psh_bind(argc, argv);
			else if (!strcmp(argv[0], "cat"))
				psh_cat(argc, argv);
			else if (!strcmp(argv[0], "exec"))
				psh_exec(argc, argv);
			else if (!strcmp(argv[0], "exit"))
				exit(EXIT_SUCCESS);
			else if (!strcmp(argv[0], "help"))
				psh_help();
			else if (!strcmp(argv[0], "history"))
				psh_history(argc, argv, &cmdhist);
			else if (!strcmp(argv[0], "kill"))
				psh_kill(argc, argv);
			else if (!strcmp(argv[0], "ls"))
				psh_ls(argc, argv);
			else if (!strcmp(argv[0], "mem"))
				psh_mem(argc, argv);
			else if (!strcmp(argv[0], "mkdir"))
				psh_mkdir(argc, argv);
			else if (!strcmp(argv[0], "mount"))
				psh_mount(argc, argv);
			else if (!strcmp(argv[0], "perf"))
				psh_perf(argc, argv);
			else if (!strcmp(argv[0], "ps"))
				psh_ps(argc, argv);
			else if (!strcmp(argv[0], "reboot"))
				psh_reboot(argc, argv);
			else if (!strcmp(argv[0], "sync"))
				psh_sync(argc, argv);
			else if (!strcmp(argv[0], "top"))
				psh_top(argc, argv);
			else if (!strcmp(argv[0], "touch"))
				psh_touch(argc, argv);
			else if (argv[0][0] == '/')
				psh_runfile(argv);
			else
				printf("Unknown command!\n");

			free(argv);
			fflush(NULL);
		}
	} while (0);

	/* Free command history */
	for (; cmdhist.hb != cmdhist.he; cmdhist.hb = (cmdhist.hb + 1) % HISTSZ)
		free(cmdhist.entries[cmdhist.hb].cmd);

	return err;
}


int main(int argc, char **argv)
{
	char *base, *dir, *path = NULL;
	oid_t oid;
	int c;

	splitname(argv[0], &base, &dir);

	if (!strcmp(base, "psh")) {
		/* Wait for root filesystem */
		while (lookup("/", NULL, &oid) < 0)
			usleep(10000);

		/* Wait for console */
		while (write(1, "", 0) < 0)
			usleep(50000);

		if (argc > 1) {
			/* Process command options */
			while ((c = getopt(argc, argv, "i:h")) != -1) {
				switch (c) {
				case 'i':
					path = optarg;
					break;

				case 'h':
				default:
					printf("usage: %s [options] [script path] or no args to run shell interactively\n", argv[0]);
					printf("  -i <script path>:  selects psh script to execute\n");
					printf("  -h:                shows this help message\n");
					return EOK;
				}
			}

			if (optind < argc)
				path = argv[optind];

			if (path != NULL)
				psh_runscript(path);
		}
		else {
			/* Run shell interactively */
			psh_run();
		}
	}
	else if (!strcmp(base, "bind")) {
		psh_bind(argc, argv);
	}
	else if (!strcmp(base, "mem")) {
		psh_mem(argc, argv);
	}
	else if (!strcmp(base, "mount")) {
		psh_mount(argc, argv);
	}
	else if (!strcmp(base, "perf")) {
		psh_perf(argc, argv);
	}
	else if (!strcmp(base, "ps")) {
		psh_ps(argc, argv);
	}
	else if (!strcmp(base, "reboot")) {
		psh_reboot(argc, argv);
	}
	else if (!strcmp(base, "sync")) {
		psh_sync(argc, argv);
	}
	else if(!strcmp(base, "top")) {
		psh_top(argc, argv);
	}
	else {
		printf("psh: %s: unknown command\n", argv[0]);
	}

	return EOK;
}