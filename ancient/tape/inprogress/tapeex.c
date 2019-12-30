/*
 * Copyright (c) 1995, 1997 by Feral Software
 *
 * $Id: tapeex.c,v 1.3 2007/09/22 04:26:40 mjacob Exp $
 *
 *  NAME
 *	tapex.c
 *
 *  DESCRIPTION
 *
 * 	Tape Excerciser program.
 *
 *  CONTENTS
 *
 *  HISTORY
 *  $Log: tapeex.c,v $
 *  Revision 1.3  2007/09/22 04:26:40  mjacob
 *  Various checkpoints.
 *
 *  Revision 1.3  1997/09/29 19:22:41  mjacob
 *  fixes
 *
 *  Revision 1.2  1997/08/19 04:46:04  mjacob
 *  *** empty log message ***
 *
 *
 *  BUGS
 *
 *  TODO
 *
 */

/*
 * Usage: tapeex [-b bs] [-f filesize] [-q] -t tapename
 *
 * where
 *	-b bs		bs is decimal size of buffer to use, specified as
 *			* N for bytes, Nk for kbytes, Nm for megabytes.
 *			Default is 32k.
 *
 *
 *	-f filesize	size of tape files, specified as * N for bytes, Nk
 *			for Kbytes, Nm for megabytes or Nr for number of
 *			records. If not specified, only one file will
 *			be created.
 *
 *	-t tapename	name of tape file to open. No default. Must be
 *			specified. Must be the no-rewind variant.
 *
 *	-q		non-verbose
 *
 * Examples:
 *
 *	tapeex -f /dev/rmt/0bn  -b 16k
 *	 - write to eot with 16k records
 *
 * Output (if not using the -q flag):
 *	Continuous single line stream of output, e.g.,
 *		  write recno      8    50.4428 ops/sec    403.5 kb/sec
 *
 */

#include <unistd.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#if	defined(sun) || defined(__NetBSD__)
#include <sys/mtio.h>
#endif
#ifdef aix
#include <sys/tape.h>
#define	mtop		stop
#define	mt_op		st_op
#define	mt_count	st_count
#define	MTIOCTOP	STIOCTOP
#define MTREW		STREW
#define MTOFFL		STOFFL
#define MTERASE		STERASE
#define MTRETEN		STRETEN
#define MTWEOF		STWEOF
#define MTFSF		STFSF
#define MTBSF		STRSF
#define MTFSR		STFSR
#define MTBSR		STRSR
#endif



static int verbose = 1;
static int blksize = 32 << 10;
static int fsize = 0;
static char *tape = NULL;

static uint64_t szarg(char *, int);
static void write_tape(char *, int);
static void tape_rewind(int);
static int tape_fm(int);

static char *usage = "\
Usage: tapeex [-b bs] [-f filesize] [-q] -t tapename\n\
\n\
where\n\
	-b bs		bs is decimal size of buffer to use, specified as\n\
			* N for bytes, Nk for kbytes, Nm for megabytes.\n\
			Default is 32k.\n\
\n\
	-f filesize	size of tape files, specified as * N for bytes, Nk\n\
			for Kbytes, Nm for megabytes or Nr for number of\n\
			records. If not specified, only one file will\n\
			be created.\n\
\n\
	-t tapename	name of tape file to open. No default. Must be\n\
			specified. Must be the no-rewind variant.\n\
\n\
	-q		non-verbose\n\
\n\
Examples:\n\
\n\
	tapeex -f /dev/rmt/0bn  -b 16k\n\
	 - write to eot with 16k records\n\
\n\
 Output (if not using the -q flag):\n\
	Continuous single line stream of output, e.g.,\n\
		  write recno      8    50.4428 ops/sec    403.5 kb/sec\n";


int
main(int a, char **v)
{
	extern char *optarg;
	extern int optind, opterr;
	char *farg;
	int c, fd;

	srand((int) (time((time_t *) 0) / getpid()));
	farg = NULL;

	while ((c = getopt(a, v, "qb:f:t:")) != -1) {
		switch (c) {
		case 'q':
			verbose = 0;
			break;
		case 'b':
			blksize = szarg(optarg, 0);
			break;
		case 'f':
			farg = optarg;
			break;
		case 't':
			tape = optarg;
			break;
		default:
			fprintf(stderr, usage);
			return (1);
		}
	}

	if (optind < a || tape == NULL) {
		fprintf(stderr, usage);
		return (1);
	}

	if (farg != NULL) {
		fsize = szarg(farg, blksize);
	}

	/*
	 * Open the file..
	 */
	fd = open(tape, O_RDWR);
	if (fd < 0) {
		perror(tape);
		return (1);
	}
	tape_rewind(fd);

	if (verbose) {
		fprintf(stdout, "%s: blocksize=%d", *v, blksize);
		if (fsize)
			fprintf(stdout, " %d bytes per file", fsize);
		fprintf(stdout, " on %s\n", tape);
	}
	write_tape(*v, fd);
	return (0);
}

}

static uint64_t
szarg(char *n)
{
    uint64_t result;
    char *q = n;

    while (isxdigit(*q)) {
        q++;
    }
    result = (uint64_t) strtoull(n, NULL, 0);
    if (*q == '\0') {
        return (result);
    }
    if (strcasecmp(q, "kib") == 0) {
        result <<= 10;
    } else if (strcasecmp(q, "mib") == 0) {
        result <<= 20;
    } else if (strcasecmp(q, "gib") == 0) {
        result <<= 30;
    } else if (strcasecmp(q, "tib") == 0) {
        result <<= 40;
    } else if (strcasecmp(q, "pib") == 0) {
        result <<= 50;
    } else if (strcasecmp(q, "k") == 0) {
        result <<= 10;
    } else if (strcasecmp(q, "m") == 0) {
        result <<= 20;
    } else if (strcasecmp(q, "g") == 0) {
        result <<= 30;
    } else if (strcasecmp(q, "t") == 0) {
        result <<= 40;
    } else if (strcasecmp(q, "p") == 0) {
        result <<= 50;
    } else if (strcasecmp(q, "kb") == 0) {
        result *= 1000;
    } else if (strcasecmp(q, "mb") == 0) {
        result *= 1000000;
    } else if (strcasecmp(q, "gb") == 0) {
        result *= 1000000000ULL;
    } else if (strcasecmp(q, "tb") == 0) {
        result *= 1000000000000ULL;
    } else if (strcasecmp(q, "pb") == 0) {
        result *= 1000000000000000ULL;
    }
    return (result);
}


#define	SAMPSIZE 100

static int eot = 0;
static void
write_tape(char *pname, int fd)
{
	static void onintr(int);
	static char *wfmt =
		"write file %4d record %12d %10.4f ops/sec %8.1f KB/sec\r";
	char *buffer;
	register int i;
	register unsigned long etime, filenum, recno, rwritten, fileno;
	struct timeval st, et;
	register double opsec, kbsec, hikbsec, hiopsec;

	buffer = malloc(blksize);
	hikbsec = hiopsec = 0.0;
	kbsec = opsec = 0.0;
	filenum = 0;
	recno = 0;
	rwritten = 0;
	eot = 0;

	if (gettimeofday(&st, (struct timezone *) 0) < 0) {
		perror("gettimeofday");
		exit(1);
	}

	(void) signal(SIGINT, onintr);
	while (!eot) {
		for (i = 0; i < blksize; i++)
			buffer[i] = rand();
		for (i = 0; eot == 0 && i < SAMPSIZE; i++) {
			if (write(fd, buffer, blksize) != blksize) {
				eot = 1;
				break;
			}
			recno++;
			rwritten++;
			if (verbose) {
				printf(wfmt, filenum, recno, opsec, kbsec);
			}
			if (fsize) {
				if ((recno * blksize) >= fsize) {
					fd = tape_fm(fd);
					filenum++;
					recno = 0;
				}
			}
		}
		if (gettimeofday(&et, (struct timezone *) 0) < 0) {
			perror("gettimeofday");
			exit(1);
		}
		etime = et.tv_sec - st.tv_sec;
		if (et.tv_usec < st.tv_usec) {
			etime--;
			etime *= 1000000;
			etime += (et.tv_usec + 1000000 - st.tv_usec);
		} else {
			etime *= 1000000;
			etime += (et.tv_usec - st.tv_usec);
		}
		opsec = (double) etime / (double) 1000000.0;
		kbsec = ((blksize * (double) SAMPSIZE) / opsec) / 1024.0;
		opsec = (double) (SAMPSIZE) / opsec;
		if (hikbsec < kbsec)
			hikbsec = kbsec;
		if (hiopsec < opsec)
			hiopsec = opsec;
		st = et;
	}
	fprintf(stdout, "\n%d records written in %d files (total = %6.1fMB)\n",
		rwritten, filenum + 1, ((double) rwritten * (double) blksize) /
		(1024.0 * 1024.0));
	fprintf(stdout, "highest ops/sec = %10.4f; highest KB/Sec = %8.1f\n",
		hiopsec, hikbsec);
}

static void
onintr(int unused)
{
	eot = 1;
}

static void
tape_rewind(fd)
{
	struct mtop op;

	op.mt_op = MTREW;
	op.mt_count = 0;
	if (ioctl(fd, MTIOCTOP, (caddr_t) &op) < 0) {
		perror("MTREWIND");
		exit(1);
		/* NOTREACHED */
	}
}

static int
tape_fm(int fd)
{
	(void) close(fd);
	fd = open(tape, O_RDWR);
	if (fd < 0) {
		perror(tape);
		exit(1);
		/* NOTREACHED */
	}
	return (fd);
}
