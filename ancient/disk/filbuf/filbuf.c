/*
 * Copyright (c) 1999, 2013 by Matthew Jacob
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
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <memory.h>
#include <ctype.h>

static uint64_t szarg(char *);
static void mcsum(char *, char *, int, off_t);
static void miscompare(char *, char *, int, off_t);
static void record_miscompare(char *, char *, int, off_t);

#define	DEFAULT_BUFLEN			(63 << 10)
#define	DEFAULT_FILESIZE		(1 << 20)

struct mc {
 struct mc *nxt;
 off_t start;
 unsigned int len;
};
struct mc *root, *tail;
extern int optind;
extern char *optarg;

int
main(int argc, char **argv)
{
	static const char *usage =
	    "usage: %s -{o,i} filename [ -S ] [ -s size[{k|m|g}] ] [ -p idpattern | -n numericvalue ]\n";
	int c, un = 0, fd, io, amt, Sflag = 0, Zflag = 0;
	unsigned long tmp = 0;
	off_t filesize, curoff;
	size_t buflen;
	char *idp, *filename, *buf, *ptr;
	io = 0;
	idp = argv[0];
	filename = NULL;
	buflen = DEFAULT_BUFLEN;
	filesize = DEFAULT_FILESIZE;
	buf = NULL;

	while ((c = getopt(argc, argv, "ZSn:p:s:o:i:")) != -1) {
		switch (c) {
		case 'Z':
			Zflag = 1;
			break;
		case 'S':
			Sflag = 1;
			break;
		case 'n':
			if (un == 1) {
				fprintf(stderr, "-n used twice?\n");
				return (1);
			}
			if (un == 2) {
				fprintf(stderr, "cannot use both -p and -n arguments\n");
				return (1);
			}
			tmp = strtoul(optarg, &buf, 0);
			if (buf == optarg) {
				fprintf(stderr, "mangled -n argument\n");
				return (1);
			}
			un = 1;
			break;
		case 'p':
			if (un == 1) {
				fprintf(stderr, "cannot use both -p and -n arguments\n");
				return (1);
			}
			if (un == 2) {
				fprintf(stderr, "-p used twice?\n");
				return (1);
			}
			idp = optarg;
			break;
		case 'i':
			if (filename != NULL) {
				fprintf(stderr, usage, argv[0]);
				return (1);
			}
			filename = optarg;
			io = 'i';
			break;
		case 'o':
			if (filename != NULL) {
				fprintf(stderr, usage, argv[0]);
				return (1);
			}
			filename = optarg;
			io = 'o';
			break;
		case 's':
			filesize = szarg(optarg);
			break;
		default:
			fprintf(stderr, usage, argv[0]);
			return (1);
		}

	}	

	if (filename == NULL) {
		fprintf(stderr, usage, argv[0]);
		return (1);
	}

	if ((buf = malloc(buflen)) == NULL) {
		perror("malloc");
		return (1);
	}

	/*
	 * Fill the buffer with a pattern we write out or compare to.
	 */
	ptr = buf;
	if (un == 1) {
		for (ptr = buf; ptr <= &buf[buflen - (sizeof (uint32_t))]; ptr += sizeof (uint32_t)) {
			uint32_t fred = tmp;
			memcpy(ptr, &fred, sizeof (uint32_t));
		}
	} else {
		c = strlen(idp);
		for (ptr = buf; ptr <= &buf[buflen-c]; ptr += c) {
			strcpy(ptr, idp);
		}
	}

	if (io == 'o') {
		fd = open(filename, O_RDWR|O_CREAT, 0644);
		if (fd < 0) {
			perror(filename);
			return (1);
		}
		for (curoff = 0; curoff < filesize; curoff += amt) {
			amt = buflen;
			if ((curoff + amt) > filesize) {
				amt = (int) (filesize - curoff);
			}
			io = write(fd, buf, amt);
			if (io != amt) {
				if (errno)
					perror("write");
				fprintf(stderr, "Wrote %d of %d\n", io, amt);
				(void) close(fd);
				return (1);
			}
		}
	} else {
		char *cmpbuf = malloc(buflen);
		if (cmpbuf == NULL) {
			perror("malloc");
			return (1);
		}
		fd = open(filename, O_RDONLY, 0);
		if (fd < 0) {
			perror(filename);
			return (1);
		}
		for (curoff = 0; curoff < filesize; curoff += amt) {
			amt = buflen;
			if ((curoff + amt) > filesize) {
				amt = (int) (filesize - curoff);
			}
			memset(cmpbuf, 0xff, amt);
			io = read(fd, cmpbuf, amt);
			if (io != amt) {
				if (errno)
					perror("read");
				fprintf(stderr, "Read %d of %d\n", io, amt);
				(void) close(fd);
				return (1);
			}
			if (memcmp(buf, cmpbuf, amt)) {
				if (Sflag) {
					mcsum(buf, cmpbuf, amt, curoff);
				} else if (Zflag) {
					record_miscompare(buf, cmpbuf, amt, curoff);
					continue;
				} else {
					miscompare(buf, cmpbuf, amt, curoff);
				}
				(void) close(fd);
				return (1);
			}
		}
	}
	(void) close(fd);
	if (Zflag && root) {
		struct mc *mc;
		int hdr = 0;

		for (mc = root; mc; mc = mc->nxt) {
			if (mc->nxt && mc->start + mc->len == mc->nxt->start) {
				mc->len += mc->nxt->len;
				mc->nxt = mc->nxt->nxt;
			}
		}
		for (mc = root; mc; mc = mc->nxt) {
			if (hdr++ == 0)
				fprintf(stdout, "%s range miscompares", filename);
			fprintf(stdout, " %lu,%u", mc->start, mc->len);
		}
		if (hdr)
			putchar('\n');
	}
	return (0);
}

static void
mcsum(char *diskbuf, char *patternbuf, int bufsize, off_t curoff)
{
	int i, j, nf = -1;
	for (i = j = 0; i < bufsize; i++) {
		if (diskbuf[i] != patternbuf[i]) {
			if (nf == -1) {
				j = i;
				nf = 0;
			}
		} else {
			if (nf == 0) {
				nf = -1;
				fprintf(stdout,
			    "miscompare from offset % 9ld to % 9ld (%d)\n",
				    (long) (curoff + j),
				    (long) (curoff + i), i - j);
			}
		}
	}
	if (nf != -1) {
		fprintf(stdout, "miscompare from offset % 9ld to % 9ld (%d)\n",
		    (long) (curoff + j),
		    (long) (curoff + i), i - j);
	}
}

static void
miscompare(char *diskbuf, char *patternbuf, int bufsize, off_t curoff)
{
	int i;
	fprintf(stdout, "\tData Miscompare Detected\n");
	fprintf(stdout, "   Offset\t Original -> Read\n");
	fprintf(stdout, "---------------------------------\n");
	for (i = 0; i < bufsize; i++) {
		if (diskbuf[i] != patternbuf[i]) {
			fprintf(stdout, "% 9ld\t 0x%02x     -> 0x%02x\n",
			    (long) (curoff + i), patternbuf[i] & 0xff,
			    diskbuf[i] & 0xff);
		}
	}
}

static void
record_miscompare(char *diskbuf, char *patternbuf, int bufsize, off_t curoff)
{
	int i;
	struct mc *mc = NULL;
	for (i = 0; i < bufsize; i++) {
		if (diskbuf[i] != patternbuf[i]) {
			if (mc == NULL) {
				mc = malloc(sizeof (*mc));
				if (mc == NULL) {
					fprintf(stderr, "cannot malloc\n");
					exit(1);
				}
				mc->start = curoff + i;
				mc->nxt = 0;
			}
		} else if (mc) {
			mc->len = curoff + i - mc->start;
			if (tail) {
				tail->nxt = mc;
			}
			if (root == NULL)
				root = mc;
			tail = mc;
			mc = NULL;
		}
	}
	if (mc) {
		mc->len = curoff + i - mc->start;
		if (tail) {
			tail->nxt = mc;
		}
		if (root == NULL)
			root = mc;
		tail = mc;
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
/*
 * Local variables:
 * c-indent-level: 8
 * c-brace-imaginary-offset: 0
 * c-brace-offset: -8
 * c-argdecl-indent: 8
 * c-label-offset: -8
 * c-continued-statement-offset: 8
 * c-continued-brace-offset: 0
 * End:
 */
