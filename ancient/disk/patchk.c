/* $Id: patchk.c,v 1.25 2013/03/08 02:03:41 mjacob Exp $ */
/*
 * Copyright (c) 1999, 2001, 2002, 2021 Matthew Jacob
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
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
 *
 */
/*
 * compile with -D_LARGEFILE_SUPPORT=1 -D_LARGEFILE64_SUPPORT -D_FILE_OFFSET_BITS=64
 */
#if    defined(sun) && defined(__SVR4)
#define    u_int64_t    uint64_t
#define    SEEK_T    off64_t
#define    SEEK    lseek64
#define    FSTAT    fstat64
#define    STAT_T    struct stat64
#define    _LARGEFILE64_SOURCE
#elif    defined(linux)
#define    OPEN    open
#define    SEEK_T    off_t
#define    SEEK    lseek
#define    FSTAT    fstat
#define    STAT_T    struct stat
#else
#define    SEEK_T    off_t
#define    SEEK    lseek
#define    FSTAT    fstat
#define    STAT_T    struct stat
#endif
#ifndef    OPEN
#define    OPEN    open
#endif
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#ifdef sun
#define    rand    lrand48
#define    srand    srand48
#ifdef    __SVR4
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
#include <sys/ioctl.h>
#include <linux/fs.h>
/* the actual ioctl, BLKGETSIZE64, in linux/fs.h, does not compile easily */
#ifdef    BLKGETSIZE64
#define BGS64   BLKGETSIZE64
#else
#define BGS64   _IOR(0x12, 114, sizeof(uint64_t))
#endif
#endif
#if    defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/disklabel.h>
#include <sys/dkio.h>
#endif
#if    defined(__bsdi__)
#include <sys/disklabel.h>
#endif
#if    defined(__FreeBSD__)
#include <sys/disk.h>
#endif
#ifdef __ultrix
extern int optind;
#endif
#ifdef    __bsdi__
#define    rand    random
#define    srand    srandom
#endif

#ifndef    O_LARGEFILE
#define    O_LARGEFILE    0
#endif

static SEEK_T szarg(char *);
static const char usage[] = "\n\
Usage: patchk TestFile BlockSize {i[nit]|c[reate]|u[se]|v[alidate]} [MaxSize]\n\
\n\
TestFile may be a disk device or a file. If specifying a regular file,\n\
and the file does not exist, both create and MaxSize must be specified.\n\
\n\
BlockSize is the size of each pattern block. It may be specified as bytes,\n\
kilobytes, or megabytes. For example, 1048576, 1024k, 1m, all specify one\n\
megabyte.\n\
\n\
One of i[nit], c[reate], u[se], or v[alidate] must be selected. Initially you must\n\
create the patterns. Later, you can rerun this with u[se], which will validate\n\
and then use the same pattern size, or v[alidate], which will just validate\n\
the patterns. You must use the same blocksize you used when creating the\n\
patterns. You can use i[nit] to just initialize and exit.\n\
\n\
MaxSize may be specified the same as BlockSize.\n\
\n\
Once the patterns are created and validated, the test shifts to randomly\n\
reading and ramdomly refreshing each pattern block. The test is continuous\n\
for the c[reate] and u[se] options, so you have to interrupt it to stop it.\n\
\n\
If the environment variable SEQUENTIAL is set, the tests after creation and\n\
validation just sequentially refresh each block.\n\
\n\
If the environment variable OVERLAPPED is set, the tests after creation and\n\
validation just sequentially read blocksize blocks, incrment one block, and continue\n\
and begin at beginning when done.\n\
\n\
";
 
int
main(int a, char **v)
{
    char *progname;
    SEEK_T seekbase, seeklim, *buffer;
    SEEK_T blksize, filesize, wloc, yloc;
    u_int64_t sb, sl;
    STAT_T st;
    int initonly = 0;
    int fd, i, j, error, oflags, sequential;

    seekbase = (SEEK_T) 0;
    oflags = 0;
    srand((int)(time((time_t *) 0)/getpid()));

    sequential = 0;
    if (getenv("SEQUENTIAL") != NULL) {
        sequential = 1;
    }
    if (getenv("OVERLAPPED") != NULL) {
        sequential = 2;
    }
    progname = v[0];
    if (a == 5) {
        filesize = szarg(v[4]);
        a--;
    } else {
        filesize = 0;
    }
    if (a != 4) {
usage:
        fprintf(stderr, usage);
        return (1);
    }
    blksize = szarg(v[2]);
    if (blksize & ((sizeof (SEEK_T)) - 1)) {
        fprintf(stderr, "blocksize must be aligned to %ld bytes\n", (long) sizeof (SEEK_T));
        return (1);
    }
    buffer = (SEEK_T *) calloc((size_t) blksize, sizeof (SEEK_T));
    if (buffer == NULL) {
        perror("malloc");
        return (1);
    }
    if (*v[3] == 'c' || *v[3] == 'i') {
        oflags = O_RDWR|O_CREAT;
    if (*v[3] == 'i')
      initonly = 1;
    } else if (*v[3] == 'u') {
        oflags = O_RDWR;
    } else if (*v[3] == 'v') {
        oflags = O_RDONLY;
    } else {
        goto usage;
    }

    fd = OPEN(v[1], oflags | O_LARGEFILE, 0666);
    if (fd < 0) {
        perror(v[1]);
        exit(1);
    }
    if (FSTAT(fd, &st) < 0) {
        perror("fstat");
        exit(1);
    }
    if (S_ISCHR(st.st_mode)) {
#if    defined(__NetBSD__) || defined(__OpenBSD__) || defined(__bsdi__)
        int part;
        struct disklabel x;

        if (ioctl(fd, DIOCGDINFO, (caddr_t) &x) < 0) {
            perror("DIOCGDINFO");
            exit(1);
        }
        seekbase = 8192;
        part = v[1][strlen(v[1]) - 1] - 'a';
        seeklim = ((SEEK_T) x.d_partitions[part].p_size) * (SEEK_T) DEV_BSIZE;
#elif    defined(__FreeBSD__)
        seekbase = 8192;
        if (ioctl(fd, DIOCGMEDIASIZE, &seeklim) < 0) {
            perror("DIOCGMEDIASIZE");
        }
#elif    defined(sun)
        struct dk_allmap x;
        int part;

        if (blksize < DEV_BSIZE) {
            fprintf(stderr, "%s: block size must be at least %d bytes on raw device\n", progname, DEV_BSIZE);
            exit(1);
        }
#if    defined(__svr4__)
        part = v[1][strlen(v[1]) - 1] - '0';
#else
        part = v[1][strlen(v[1]) - 1] - 'a';
#endif
        if (ioctl(fd,  DKIOCGAPART, (caddr_t) &x) < 0) {
            perror("DKIOCGAPART");
            exit(1);
        }
        seekbase = 8192;
        seeklim = ((SEEK_T) x.dka_map[part].dkl_nblk) * (SEEK_T) DEV_BSIZE;
#else
        seeklim = (SEEK_T) 1;
#endif
        if (seekbase < blksize)
                seekbase = blksize;
        if (filesize && filesize < seeklim)
            seeklim = filesize;
    } else if (S_ISREG(st.st_mode)) {
        seekbase = 0;
        seeklim = st.st_size;
        if (seeklim == 0 && filesize) {
            seeklim = filesize;
        }
    } else {
#ifdef linux
        seeklim = 0;
        {
            uint64_t ss;
            if (ioctl(fd, BGS64, (caddr_t) &ss) < 0) {
                perror("BLKGETSIZE64");
                exit(1);
            }
            seeklim = ss;
        }
        if (seekbase < blksize)
            seekbase = blksize;
        if (filesize && filesize < seeklim)
            seeklim = filesize;
#else
        fprintf(stderr, "%s: is not a raw device\n", progname);
        return (1);
#endif
    }

    /*
     * Truncate to lower block boundary.
     * We cannot assume a power of two blocksize.
     */
    seeklim -= (seeklim % blksize);

    if (seeklim < blksize) {
        printf("%s seek limit %lld less than blocksize %lld\n", progname, (long long) seeklim, (long long) blksize);
        exit(1);
    }
    if (seeklim <  2 * (seekbase+blksize)) {
        fprintf(stderr, "%s: botch, seeklim (%lld) < seekbase + blksize (%lld)\n", progname, (long long) seeklim, (long long) (seekbase + blksize));
        exit(1);
    }

    sb = (u_int64_t) seekbase;
    sl = (u_int64_t) seeklim;
    fprintf(stdout, "%s: Seek base %lx%08lx Seek lim %lx%08lx blocksize %ld\n", progname, (long) (sb >> 32), (long) (sb & 0xFFFFFFFF),
        (long) (sl >> 32), (long) (sl & 0xFFFFFFFF), (long) blksize);
    wloc = SEEK(fd, (SEEK_T) seekbase, 0);
    if (wloc < (SEEK_T) 0) {
        perror("seek");
        exit (1);
    }

    if (*v[3] == 'c' || *v[3] == 'i') {
        fprintf(stdout, "Creating Patterns...");
        fflush(stdout);
        for (wloc = seekbase; wloc < seeklim; wloc += blksize) {
            sb = (u_int64_t) wloc;
            for (yloc = wloc, i = 0; i < (blksize/sizeof (SEEK_T)); i += (512/sizeof (SEEK_T)), yloc++) {
                for (j = i; j < i + (512/sizeof (SEEK_T)); j++) {
                    buffer[j] = yloc;
                }
            }
            if ((i = write(fd, (char *)buffer, (int) blksize)) != blksize) {
                if (errno)
                    perror("write");
                fprintf(stderr, "write returned %d at offset 0x%lx%08lx\n", i, (long) (sb >> 32), (long) (sb & 0xFFFFFFFF));
                exit (1);
            }
        }
        wloc = SEEK(fd, (SEEK_T) seekbase, 0);
        if (wloc < (SEEK_T) 0) {
            perror("seek");
            exit (1);
        }
    }
    if (*v[3] == 'i') {
        fprintf(stdout, "Done\n");
        exit (0);
    }
    fprintf(stdout, "Checking Patterns...");
    fflush(stdout);
    for (wloc = seekbase; wloc < seeklim; wloc += blksize) {
        sb = (u_int64_t) wloc;
        memset(buffer, 0xee, blksize);
        if ((i = read(fd, (char *)buffer, blksize)) != blksize) {
            if (errno)
                perror("read");
            fprintf(stderr, "read returned %d at offset 0x%lx%08lx\n", i, (long) (sb >> 32), (long) (sb & 0xFFFFFFFF));
            exit (1);
        }
        for (yloc = wloc, error = i = 0; i < (blksize/sizeof (SEEK_T)); i += (512/sizeof (SEEK_T)), yloc++) {
            for (j = i; j < i + (512/sizeof (SEEK_T)); j++) {
                buffer[j] = yloc;
                if (buffer[j] != yloc) {
                    sb = yloc;
                    sl = (u_int64_t) buffer[j];
                    error++;
                    fprintf(stderr, "compare error at block loc 0x%lx%08lx offset %d got %lx%08lx\n", (long) (sb >> 32),
                        (long) (sb & 0xFFFFFFFF), i, (long) (sl >> 32), (long) (sl & 0xFFFFFFFF));
                }
            }
            if (error)
                exit (1);
        }
    }

    if (sequential) {
        fprintf(stdout, "Sequentially Rechecking Patterns\n");
        wloc = seekbase;
    } else {
        fprintf(stdout, "Randomly Checking Patterns\n");
    }

    while (1) {
        SEEK_T sloc;

        if (sequential == 0) {
            wloc = (SEEK_T) rand();
        }

        /*
         * We cannot assume blksize is a power of two.
         */
        if (wloc < seekbase) {
            continue;
        }
        if (wloc > seeklim) {
            continue;
        }
        if (wloc+blksize > seeklim) {
            if (sequential)
                wloc = seekbase;
            continue;
        }
        sloc = SEEK(fd, wloc, 0);
        if (sloc < (SEEK_T) 0) {
            perror("seek");
            exit (1);
        }
        if (sloc != wloc) {
            if (errno)
                perror("seek2");
            fprintf(stderr, "wanted to seek to 0x%llx and got to 0x%llx instead\n", (long long) wloc, (long long) sloc);
            continue;
        }
        sb = (u_int64_t) wloc;
        if ((oflags & O_RDWR) && (sequential == 1 || (rand() & 1))) {
            sb = (u_int64_t) wloc;
            for (yloc = wloc, i = 0; i < (blksize/sizeof (SEEK_T)); i += (512/sizeof (SEEK_T)), yloc++) {
                for (j = i; j < i + (512/sizeof (SEEK_T)); j++) {
                    buffer[j] = yloc;
                }
            }
            if ((i = write(fd, (char *)buffer, blksize)) != blksize) {
                fprintf(stderr, "\n");
                if (errno)
                    perror("write");
                fprintf(stderr, "write returned %d at offset 0x%lx%08lx\n", i, (long) (sb >> 32), (long) (sb & 0xFFFFFFFF));
                exit (1);
            }
            continue;
        }
        memset(buffer, 0xff, blksize);
        if ((i = read(fd, (char *)buffer, blksize)) != blksize) {
            fprintf(stderr, "\n");
            if (errno)
                perror("read");
            fprintf(stderr, "read returned %d at offset 0x%lx%08lx\n", i, (long) (sb >> 32), (long) (sb & 0xFFFFFFFF));
            exit (1);
        }
        for (yloc = wloc, error = i = 0; i < (blksize/sizeof (SEEK_T)); i += (512/sizeof (SEEK_T)), yloc++) {
            for (j = i; j < i + (512/sizeof (SEEK_T)); j++) {
                buffer[j] = yloc;
                if (buffer[j] != yloc) {
                    sb = yloc;
                    sl = (u_int64_t) buffer[j];
                    error++;
                    fprintf(stderr, "compare error at block loc 0x%lx%08lx offset %d got %lx%08lx\n", (long) (sb >> 32),
                        (long) (sb & 0xFFFFFFFF), i, (long) (sl >> 32), (long) (sl & 0xFFFFFFFF));
                }
            }
            if (error)
                exit (1);
        }
        if (error)
            exit (1);
        if (sequential == 1) {
            wloc += blksize;
            if (wloc == seeklim) {
                wloc = seekbase;
            }
        } else if (sequential == 2) {
            wloc++;
            if (wloc == seeklim) {
                wloc = seekbase;
            }
        }
    }
}

static SEEK_T
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
 * vim:ts=4:sw=4:expandtab
 */
