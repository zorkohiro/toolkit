/*
 * Copyright (c) 1998 by Matthew Jacob
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 * compile with -D_LARGEFILE_SUPPORT=1 -D_LARGEFILE64_SUPPORT -D_FILE_OFFSET_BITS=64
 */

#ident	"$Id: diskex.c,v 1.7 2008/01/29 17:17:55 mjacob Exp $"
#ifdef convex
#include <sys/types.h>
extern int optind;
extern int getopt(int, char **, const char *);
#define	SEEK_T	off64_t
#define	SEEK	lseek64
#define	FSTAT	fstat64
#define	STAT_T	stat64_t
#else
#define	SEEK_T	off_t
#define	SEEK	lseek
#define	FSTAT	fstat
#define	STAT_T	struct stat
#endif
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#ifdef sun
#ifdef	__SVR4
#include <sys/dkio.h>
#else
#include <sun/dkio.h>
extern int gettimeofday(struct timeval *, struct timezone *);
extern void bzero(char *, int);
extern int strtol(const char *, char **, int);
#endif
extern int optind;
#endif
#ifdef __linux__
#include <stdint.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
/* the actual ioctl, BLKGETSIZE64, in linux/fs.h, does not compile easily */
// #define BGS64   _IOR(0x12, 114, sizeof(uint64_t))
#define	BGS64	BLKGETSIZE64
#endif
#ifdef	convex
#include <sys/ioctl.h>
#include <interfaces/io_if/scsi/scsi.h>
#endif
#if	defined(__NetBSD__) || defined(__FreeBSD__) || defined(__OpenBSD__)
#include <sys/disklabel.h>
#if	defined(__NetBSD__)
#include <sys/dkio.h>
#endif
static struct disklabel fred;
#endif
#ifdef __ultrix
extern int optind;
#endif


#ifndef	O_LARGEFILE
#define	O_LARGEFILE	0
#endif
static int sequential = 1;
static int verbose = 1;
static int tflag = 0;
#ifdef	__linux__
static int zflag = 0;
#endif
static int wpercent = 0;
static SEEK_T rfsize;
static SEEK_T fsize;
static SEEK_T seekbase;
static SEEK_T seeklim;
static SEEK_T cresize;
static unsigned long sampsize = 100;
static unsigned long blksize = 4096;
static char *filename = "diskex.data";
static char *buffer = NULL;

static void create_file(int, SEEK_T);
static SEEK_T szarg(char *);
static void operate(char *, int);

static char *usage =
"$Id: diskex.c,v 1.7 2008/01/29 17:17:55 mjacob Exp $"
"Usage:\n"
"diskex [-tzqr] [-b bs] [-w pct] [-f filename] [-c size] [-s size] [-S sampsize]\n"
"	-z		linux only) flush buffer caches every sampsize ops\n"
"			(more closely approximates real device speeds)\n"
"\n"
"	-q		non-verbose\n"
"\n"
"	-r		random vs. sequential access. Random is w.r.t.\n"
"			to blocksize. Defaults to sequential access.\n"
"\n"
"	-b bs		bs is decimal size of buffer to use, specified as\n"
"			* N for bytes, Nk for kbytes, Nm for megabytes.\n"
"			Default is 4kb.\n"
"\n"
"	-w pct 		Writes vs. Reads. The argument pct is the\n"
"			percentage of writes vs. reads.\n"
"\n"
"	-f filename	name of file to open. Defaults to the name\n"
"			\"diskex.data\".\n"
"\n"
"	-c size		For the case where you wish to (re)create\n"
"			the data file.\n"
"\n"
"	-s size		Size of test area within file to exercise.\n"
"			If not specified, the entire file is used,\n"
"			modulo block size.\n"
"\n"
" 	-S sampsize	Size of sample interval (default 100)\n"
"\n"
"	-t		perform comparison tests on data- not useful\n"
"			for performance, but can be used to detect\n"
"			file corruption. Not exhaustive. Requires\n"
"			sequential operation.\n"
"\n"
"	-h		Print this message\n"
"\n"
" Examples:\n"
"\n"
"	diskex -f /dev/rsd3c -s 100m -r -b 16k\n"
"	 - execute random reads in the front 100 megabytes of /dev/rsd3c\n"
"	   with a blocksize of 16kbytes.\n"
"\n"
"	diskex -f /tmp/bob.data -c 25m -w 25\n"
"	 - create /tmp/bob.data (25 megabytes) and operate on it sequentially\n"
"	  with 25%% of the operations as writes.\n"
"\n"
" Output (if not using the -q flag):\n"
"	Continuous single line stream of output, e.g.,\n"
"		  read bno      8    50.4428 ops/sec    403.5 kb/sec\n";

int
main(int a, char **v)
{
    extern char *optarg;
    STAT_T st;
    int c, fd;

    srand((int)(time((time_t *) 0)/getpid()));

    while ((c = getopt(a, v, "htzqrb:w:f:s:c:S:")) != -1) {
	switch (c) {
	case 't':
	    tflag = 1;
	    break;
#ifdef	__linux__
	case 'z':
	    zflag = 1;
	    break;
#endif
	case 'q':
	    verbose = 0;
	    break;
	case 'r':
	    sequential = 0;
	    break;
	case 'S':
	    sampsize = (unsigned long) szarg(optarg);
	    break;
	case 'b':
	    blksize = (unsigned long) szarg(optarg);
	    if (blksize & (blksize - 1)) {
		fprintf(stderr, "%s: blksize must be a power of two\n", *v);
		exit(1);
	    }
	    break;
	case 'w':
	    wpercent = atoi(optarg);
	    if (wpercent < 0 || wpercent > 100) {
		fprintf(stderr, "%s: bad -w value (%d)\n", *v, wpercent);
		exit(1);
	    }
	    break;
	case 'f':
	    filename  = optarg;
	    break;
	case 's':
	    fsize = szarg(optarg);
	    break;
	case 'c':
	    cresize = szarg(optarg);
	    break;
	case 'h':
	default:
	    fprintf(stderr, usage);
	    exit(1);
	}
    }
    if (optind < a) {
	fprintf(stderr, "More options? Try '-h' for usage\n");;
	exit(1);
    }

    if (wpercent == 100 && tflag) {
	fprintf(stderr, "You cannot ask for data checking with 100%% writes\n");
	exit(1);
    }

    if (sequential == 0 && tflag) {
	fprintf(stderr, "You cannot ask for data checking with random I/O\n");
	exit(1);
    }

    buffer = malloc(blksize);
    if (buffer == NULL) {
	perror("malloc");
	exit(1);
    }

    /*
     * Open the file..
     */
    if (cresize != (SEEK_T) 0) {
	
	fd = open(filename, O_CREAT|O_TRUNC|O_WRONLY|O_LARGEFILE, 0666);
	if (fd < 0) {
	    perror(filename);
	    exit(1);
	}
	create_file(fd, cresize);
	(void) close(fd);
	fsize = cresize;
    }
    fd = open(filename, O_RDWR|O_LARGEFILE, 0666);
    if (fd < 0) {
	perror(filename);
	exit(1);
    }
    /*
     * See if it is a device rather than a 'regular' file...
     */
    if (FSTAT(fd, &st) < 0) {
	perror("fstat");
	exit(1);
    }
    if (S_ISBLK(st.st_mode)) {
#if	defined(__NetBSD__) || defined(__FreeBSD__) || defined(__OpenBSD__)
	char *makeraw(char *);
	int rfd, part;

	if ((rfd = open(makeraw(filename), O_RDONLY, 0666)) < 0) {
	    printf("Can't deal with cooked devices\n");
	    exit(1);
	}
	if (ioctl(rfd, DIOCGDINFO, (caddr_t) &fred) < 0) {
	    perror("DIOCGDINFO_raw");
	    exit(1);
	}
	seekbase = (wpercent == 0) ? 0 : 8192;
	part = filename[strlen(filename) - 1] - 'a';
	rfsize = seeklim =
	    ((SEEK_T) fred.d_partitions[part].p_size) * (SEEK_T) DEV_BSIZE;
	(void) close(rfd);
#elif	defined(sun)
	struct dk_allmap x;
	char *makeraw(char *);
	int rfd, part;

	if ((rfd = open(makeraw(filename), O_RDONLY, 0666)) < 0) {
	    printf("Can't deal with cooked devices\n");
	    exit(1);
	}
#if	defined(__svr4__)
	part = filename[strlen(filename) - 1] - '0';
#else
	part = filename[strlen(filename) - 1] - 'a';
#endif
	if (ioctl(rfd, DKIOCGAPART, (caddr_t) &x) < 0) {
	    perror("DKIOCGAPART_raw");
	    exit(1);
	}
	seekbase = (wpercent == 0) ? 0 : 8192;
	rfsize = seeklim =
	    ((SEEK_T) x.dka_map[part].dkl_nblk) * (SEEK_T) DEV_BSIZE;
	(void) close(rfd);
#elif	defined(linux)
	{
		uint64_t ss;
		if (ioctl(fd, BGS64, (caddr_t) &ss) < 0) {
		    perror("BLKGETSIZE64");
		    exit(1);
		}
		seeklim = ss;
	}
	rfsize = seeklim;
#else
	rfsize = seeklim = (SEEK_T) 1;
#endif
	if (tflag)
	    create_file(fd, rfsize);
    } else if (S_ISCHR(st.st_mode)) {
#if	defined(__NetBSD__) || defined(__FreeBSD__) || defined(__OpenBSD__)
	int part;

	if (ioctl(fd, DIOCGDINFO, (caddr_t) &fred) < 0) {
	    perror("DIOCGDINFO");
	    exit(1);
	}
	seekbase = (wpercent == 0) ? 0 : 8192;
	part = filename[strlen(filename) - 1] - 'a';
	rfsize = seeklim =
	    ((SEEK_T) fred.d_partitions[part].p_size) * (SEEK_T) DEV_BSIZE;
#elif	defined(sun)
	struct dk_allmap x;
	int part;

	if (blksize < DEV_BSIZE) {
	    fprintf(stderr, "%s: block size must be at least %d bytes on "
		    "raw device\n", *v, DEV_BSIZE);
	    exit(1);
	}
#if	defined(__svr4__)
	part = filename[strlen(filename) - 1] - '0';
#else
	part = filename[strlen(filename) - 1] - 'a';
#endif
	if (ioctl(fd,  DKIOCGAPART, (caddr_t) &x) < 0) {
	    perror("DKIOCGAPART");
	    exit(1);
	}
	seekbase = (wpercent == 0) ? 0 : 8192;
	rfsize = seeklim =
	    ((SEEK_T) x.dka_map[part].dkl_nblk) * (SEEK_T) DEV_BSIZE;
#elif	defined(convex)
	struct topology top;
	seeklim = 0;
	if (ioctl(fd, SIOC_READ_TOPOLOGY, (caddr_t)&top) >= 0) {
		seeklim = (SEEK_T) top.partition[st.st_rdev & 0xf].size *
		    (SEEK_T) DEV_BSIZE;
	}
	rfsize = seeklim;
#else
	rfsize = seeklim = (SEEK_T) 1;
#endif
	if (tflag)
	    create_file(fd, rfsize);
    } else {
	rfsize = seeklim = st.st_size;
    }

    if (seeklim < (SEEK_T) 0) {
	printf("%s too big for lseek(2) call\n", filename);
	exit(1);
    }

    if (fsize) {
        if (fsize > seeklim)
	    fsize = seeklim;
    	else if (seeklim > fsize)
	    seeklim = fsize;
    } else {
	fsize = seeklim;
    }

    seeklim &= ~(blksize-1);
    if (seeklim < seekbase) {
	fprintf(stderr, "%s: botch, seeklim (%ld) < seekbase (%ld)\n",
		*v, seeklim, seekbase);
	exit(1);
    }
    if (verbose) {
	long long fs, rs, sb, sl;
	fs = (long long) fsize;
	rs = (long long) rfsize;
	sb = (long long) seekbase;
	sl = (long long) seeklim;
	fprintf(stdout, "%s: fsize %lx%08lx (real %lx%08lx) Seek base "
		" %lx%08lx Seek lim %lx%08lx\n", *v,
		(long) (fs >> 32LL), (long) (fs & 0xFFFFFFFF),
		(long) (rs >> 32LL), (long) (rs & 0xFFFFFFFF),
		(long) (sb >> 32LL), (long) (sb & 0xFFFFFFFF),
		(long) (sl >> 32LL), (long) (sl & 0xFFFFFFFF));
	fprintf(stdout, "%s: file %s bs %ld seq %d pct writes: %d\n",
		*v, filename, blksize, sequential, wpercent);
    }
    operate(*v, fd);
    exit(0);
}

static void
create_file(int fd, SEEK_T cresize)
{
    static int created = 0;
    unsigned long x = 0, *lp;
    if (created)
	return;
    created++;
    if (cresize && verbose) {
	long long j = (long long) cresize;
	fprintf(stdout, "creating file of size %lld bytes\n", j);
    }
    while (cresize > 0) {
	int amt = cresize > blksize? blksize: cresize;
	memset(buffer, 0, amt);
	if (tflag) {
	    lp = (unsigned long *) buffer;
	    while (lp < (unsigned long *) &buffer[amt]) {
		*lp++ = x;
		x += sizeof (unsigned long);
	    }
	}
	if (write(fd, buffer, (int) amt) != (int) amt) {
	    perror(filename);
	    exit(1);
	}
	cresize -= (SEEK_T) amt;
    }
}

static SEEK_T
szarg(char *n)
{
    SEEK_T result;
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

static void
operate(char *pname, int fd)
{
    register int i;
    register unsigned long offset, etime;
    char *optype;
    unsigned long *opvec;
    struct timeval st, et;
    register double opsec, kbsec;

    offset = seekbase;
    kbsec = opsec = 0;

    if (gettimeofday(&st, (struct timezone *) 0) < 0) {
	perror("gettimeofday");
	exit(1);
	/* NOTREACHED */
    }
    opvec = (unsigned long *) malloc(sampsize * sizeof (unsigned long));
    if (opvec == NULL) {
	fprintf(stderr, "%s: unable to malloc opvec for sampsize %ld\n",
	    pname, sampsize);
	exit(1);
	/* NOTREACHED */
    }
    optype = (char *) malloc(sampsize * sizeof (char));
    if (optype == NULL) {
	fprintf(stderr, "%s: unable to malloc optype for sampsize %ld\n",
	    pname, sampsize);
	exit(1);
	/* NOTREACHED */
    }

#if	defined(sun) && defined(__svr4__) && defined(__FreeBSD__)
    setbuf(stdout, NULL);
#endif
    for (;;) {
	for (i = 0; i < sampsize; i++) {
	    if (sequential) {
		if (offset + blksize > seeklim)
		    offset = seekbase;
		opvec[i] = offset;
		offset += blksize;
	    } else {
		do {
		    opvec[i] = rand() % seeklim;
		} while (opvec[i] < seekbase || opvec[i] >= seeklim - blksize);
	    }
	    if ((float) i < (((float) wpercent / 100.0) * (float) sampsize))
		optype[i] = 1;
	    else
		optype[i] = 0;
	}

	for (i = 0; i < sampsize; i++) {
	    if (SEEK(fd, (off_t) opvec[i], 0) < 0) {
		fprintf(stderr, "%s: SEEK fails, errno %d\n", pname, errno);
		return;
	    }
	    if (optype[i]) {
		if (tflag) {
		    unsigned long x = opvec[i], *lp = (unsigned long *) buffer;
		    while (lp < (unsigned long *) &buffer[blksize]) {
			*lp++ = x;
			x += sizeof (unsigned long);
		    }
		}
		if (write(fd, buffer, blksize) != blksize) {
		    fprintf(stderr,
			"\nWrite Error at location 0x%lx (%ld): %s\n",
			opvec[i], opvec[i], strerror(errno));
		    return;
		}
	    } else {
		if (read(fd, buffer, blksize) != blksize) {
		    fprintf(stderr,
			"\nRead Error at location 0x%lx (%ld): %s\n",
			opvec[i], opvec[i], strerror(errno));
		    return;
		}
		if (tflag) {
		    unsigned long y, x, *lp;
		    x = opvec[i];
		    lp = (unsigned long *) buffer;
		    while (lp < (unsigned long *) &buffer[blksize]) {
			if ((y = *lp++) != x) {
			    fprintf(stderr, "\n\nCOMPARE ERROR AT OFFSET 0x%lx "
				"GOT 0x%lx\n\n", x, y);
			    return;
			}
			x += sizeof (unsigned long);
		    }
		}
	    }
	    if (verbose) {
		printf("%6s bno %6ld %10.4f ops/sec %8.1f kb/sec\r",
		       optype[i]? "write" : "read", opvec[i] / blksize,
		       opsec, kbsec);
	    }
	}
	if (gettimeofday(&et, (struct timezone *) 0) < 0) {
	    perror("\ngettimeofday");
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
	kbsec = ((blksize * (double) sampsize) / opsec) / 1024.0;
	opsec = (double) sampsize / (double) opsec;
	st = et;
#if	defined(linux)
	if (zflag)
	    (void) ioctl(fd, BLKFLSBUF, 0);
#endif
    }
}

#if	defined(sun) || defined(convex) || defined(__NetBSD__) || \
	defined(__FreeBSD__) || defined(__OpenBSD__)
#include <memory.h>
char *
makeraw(char *fn)
{
    register char *ns, *p;
    
    ns = malloc(strlen(fn) + 2);
    memset(ns, 0, strlen(fn) + 2);

#if	defined(sun) && defined(__svr4__)
    if (strncmp(fn, "/dev/dsk", 8) == 0) {
	strcpy(ns, "/dev/rdsk");
	strcat(ns, fn+8);
	return (ns);
    } else if (strncmp(fn, "dsk", 3) == 0) {
	strcpy(ns, "rdsk");
	strcat(ns, fn+3);
	return (ns);
    }
#endif
    p = strrchr(fn, '/');
    if (p == (char *) 0) {
	ns[0] = 'r';
	(void) strcpy(&ns[1], fn);
    } else {
	p++;
	(void) strncpy(ns, fn, p - fn);
	(void) strcat(ns, "r");
	(void) strcat(ns, p);
    }
    return (ns);
}
#endif
/*
 * ---------------------------------------------------------------------------
 * Local variables:
 * c-indent-level: 4
 * c-brace-imaginary-offset: 0
 * c-brace-offset: -4
 * c-argdecl-indent: 4
 * c-label-offset: -4
 * c-continued-statement-offset: 4
 * c-continued-brace-offset: 0
 * End:
 */
