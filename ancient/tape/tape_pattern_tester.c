/* $Id: tape_pattern_tester.c,v 1.8 2007/09/22 04:26:40 mjacob Exp $ */
/*
 * Copyright (c) 2000, 2021 by Matthew Jacob.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *   notice immediately at the beginning of the file, without modification,
 *   this list of conditions, and the following disclaimer.
 *
 *   2. The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/mtio.h>

#ifndef roundup
#define	roundup(x, y)	((((x)+((y)-1))/(y))*(y))  /* to any y */
#endif
#ifdef	__GNUC__
#define	INLINE	__inline
#else
#define	INLINE
#endif
#ifdef	sun
#define	u_int64_t	uint64_t
#endif

static struct mtop mtop;
static int myeotcnt = 0;


static uint64_t szarg(char *);
static INLINE void filbuf(void *, int, int, int);
static INLINE int chkbuf(void *, int, int, int);
static int rewind_tape(char *);

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

static INLINE void
filbuf(void *buffer, int fileno, int recno, int blksiz)
{
	int i;
	u_int64_t *lb = buffer;
	u_int64_t tseed = ((u_int64_t)fileno << 40) | ((u_int64_t)recno << 24);
	for (i = 0; i < (blksiz / (sizeof (u_int64_t))); i++) {
		lb[i] = tseed | i;
	}
}

static INLINE int
chkbuf(void *buffer, int fileno, int recno, int blksiz)
{
	register int i;
	u_int64_t *lb = buffer;
	u_int64_t tseed = ((u_int64_t)fileno << 40) | ((u_int64_t)recno << 24);
	for (i = 0; i < (blksiz / (sizeof (u_int64_t))); i++) {
		if (lb[i] != (tseed | i)) {
			fprintf(stdout,
			    "compare error at file %d, record %d, offset %d\n",
			    fileno, recno, i * sizeof (u_int64_t));
			fprintf(stdout, "Should be 0x%llx, got 0x%llx\n",
			    tseed | i, lb[i]);
			return (i+1);
		}
	}
	return (0);
}

static int
rewind_tape(char *file)
{
	int fd = open(file, O_RDONLY);
	if (fd < 0) {
		perror(file);
		return (-1);
	}
	mtop.mt_op = MTREW;
	mtop.mt_count = 1;
	if (ioctl(fd, MTIOCTOP, &mtop) < 0) {
		perror("MTREW");
		return (-1);
	}
	if (close(fd) < 0) {
		perror("close");
		return (-1);
	}
	return (0);
}

int
main(int argc, char **argv)
{
	const char *usage = "[ -v ] [ -b blksize ] [ -r blocks per file ] "
	    "[ -n number-of-files ] [ -E #filemarks@eot ] -f "
	    "no-rewinding-tape-drive";
	time_t stime, etime;
	int blksize, recperfile, nfiles;
	char *file, *bp;
	u_int64_t nw, nr;
	int fd, c, wrecno, wfileno, rrecno, rfileno, verbose, last_recno;
	int w, r, lastw, lastr, nzr;

	nw = nr = 0;
	lastw = lastr = 0;
	w = wrecno = rrecno = last_recno = 0;
	nfiles = -1;
	recperfile = 1000;
	blksize = 32 << 10;
	file = NULL;
	verbose = 0;

	while ((c = getopt(argc, argv, "E:b:r:n:f:v")) != -1) {
		switch (c) {
		case 'E':
			myeotcnt = szarg(optarg);
			break;
		case 'b':
			blksize = szarg(optarg);
			break;
		case 'r':
			recperfile = atoi(optarg);
			break;
		case 'n':
			nfiles = atoi(optarg);
			break;
		case 'f':
			file = optarg;
			break;
		case 'v':
			verbose++;
			break;
		default:
			fprintf(stderr, "%s: %s\n", argv[0], usage);
			exit(1);
		}
	}
	setbuf(stdout, NULL);

	/*
	 * Make sure tape can be opened (and rewound)
	 */

	if (file == NULL) {
		fprintf(stderr, "%s: %s\n", argv[0], usage);
		exit(1);
	}
	if (verbose)
		fprintf(stdout, ".......Rewind Tape\n");
	if (rewind_tape(file) < 0) {
		exit(1);
	}

	/*
 	 * Allocate Buffer
	 */

	blksize &= ~(sizeof (u_int64_t) - 1);
	bp = calloc(blksize / sizeof (u_int64_t), sizeof (u_int64_t));
	if (bp == NULL) {
		perror("calloc");
		exit(1);
	}

	/*
	 * Do write pass.
	 */
	if (verbose)
		fprintf(stdout, "........Write Pass\n");
	time(&stime);
	wfileno = 0;
	for (;;) {
		fd = open(file, O_RDWR);
		if (fd < 0) {
			perror(file);
			w = 0;
			break;
		}
		for (wrecno = 0; wrecno < recperfile; wrecno++) {
			filbuf(bp, wfileno, wrecno, blksize);
			if ((w = write(fd, bp, blksize)) > 0) {
				nw += w;
				lastw = w;
			}
			if (verbose > 2) {
				fprintf(stdout,
				    "Wrote File %d Record %d w=%d\n", wfileno,
				    wrecno, w);
			}
			if (w != blksize) {
				break;
			}
		}
		if ((verbose && ((wfileno+1) & 0xf) == 0) || verbose > 1) {
			fprintf(stdout,
			    "End of File %d (%lld total bytes written)\n",
			    wfileno, nw);
		}
		if (myeotcnt && (w != blksize || (nfiles > 0 &&
		    wfileno == nfiles-1))) {
			if (verbose) {
				fprintf(stdout, "....Writing %d FMKs\n",
				    myeotcnt);
			}
			mtop.mt_op = MTWEOF;
			mtop.mt_count = myeotcnt;
			if (ioctl(fd, MTIOCTOP, &mtop) < 0) {
				perror("MTWEOF");
				return (-1);
			}
			break;
		}
		if (w != blksize) {
			(void) close(fd);
			break;
		}
		if (close(fd) < 0) {
			perror("close");
			w = 0;
			break;
		}
		if (nfiles > 0 && wfileno == nfiles-1) {
			break;
		}
		wfileno++;
	}
	time(&etime);
	if (w < 0) {
		perror("write");
		w = 0;
	}
	fprintf(stdout,
	    "WEOT at File %d Record %d Offset %d (%lld total bytes written)\n",
	    wfileno, wrecno, w, nw);
	etime -= stime;
	if (etime)
		fprintf(stdout, "Elapsed Seconds: %ld; Data Rate: %gMB/s\n",
		    (long) etime, ((double) (nw >> 20)) / ((double) etime));
	else
		fprintf(stdout, "Elapsed Seconds: 0\n");

	if (wfileno == 0 && wrecno == 0)
		exit(1);


	/*
	 * Rewind tape, if needed.
	 */
	if (verbose)
		fprintf(stdout, ".......Rewind Tape\n");
	if (myeotcnt) {
		mtop.mt_op = MTREW;
		mtop.mt_count = 1;
		if (ioctl(fd, MTIOCTOP, &mtop) < 0) {
			perror("MTREW");
			exit(-1);
		}
	} else {
		if (rewind_tape(file) < 0) {
			exit(1);
		}
	}
	/*
	 * Do read pass
	 */
	if (verbose)
		fprintf(stdout, ".........Read Pass\n");
	time(&stime);

	for (nzr = 0, rfileno = 0;; rfileno++) {
		/*
		 * If this is the first file, and we were doing our own
		 * EOT management, don't open the file, as it's open
		 * already.
		 */
		if (!(myeotcnt && rfileno == 0)) {
			fd = open(file, O_RDONLY);
			if (fd < 0) {
				perror(file);
				r = 0;
				break;
			}
		}
		for (rrecno = 0;; rrecno++) {
			if ((r = read(fd, bp, blksize)) > 0) {
				last_recno = rrecno;
				nzr = 0;
				nr += r;
				lastr = r;
				if (chkbuf(bp, rfileno, rrecno, r)) {
					exit(1);
				}
			}
			if (verbose > 2) {
				fprintf(stdout,
				    "Read File %d Record %d r=%d\n", rfileno,
				    rrecno, r);
			}
			if (r != blksize) {
				break;
			}
			if (rrecno > recperfile) {
				fprintf(stdout, "read more records per file "
				    "(%d) than written (%d)\n", rrecno,
				    recperfile);
				exit(1);
			}
		}
		if ((verbose && ((rfileno+1) & 0xf) == 0) || verbose > 1) {
			fprintf(stdout,
			    "End of File %d (%lld total bytes read)\n",
			    rfileno, nr);
		}
		/*
		 * We can't break out of this loop on just plain
		 * zero length reads- because those signal an
		 * EOF condition- not EOT. We have to wait until
		 * we get an error or two zero length reads in a row
		 */
		if (r == 0) {
			if (nzr++) {
				(void) close(fd);
				break;
			} else {
				rrecno = last_recno;
			}
		} else if (r < 0) {
			if (rrecno == 0)
				rrecno = last_recno;
			(void) close(fd);
			break;
		}
		if (close(fd) < 0) {
			perror("close");
			r = 0;
			break;
		}
	}
	time(&etime);
	if (r < 0) {
		perror("read");
		r = 0;
	}
	fprintf(stdout,
	    "REOT at File %d Record %d Offset %d (%lld total bytes read)\n",
	    rfileno, rrecno, r, nr);
	etime -= stime;
	fprintf(stdout, "Elapsed Seconds: %ld: Data Rate: %gMB/s\n",
	    (long) etime, ((double) (nr >> 20) / ((double) etime)));
	return (0);
}
