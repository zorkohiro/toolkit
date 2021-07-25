/* $Id: genpat.c,v 1.2 2013/04/10 01:52:28 mjacob Exp $ */
/*
 * Copyright (c) 2013, 2021 by Matthew Jacob
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
#define _GNU_SOURCE 1
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <time.h>
#ifdef    BLKGETSIZE64
#define BGS64   BLKGETSIZE64
#else
#define BGS64   _IOR(0x12, 114, sizeof(uint64_t))
#endif

#define MYBLK   (1 << 20)

int
main(int a, char **v)
{
    int fd, i, ret, w;
    uint64_t val;
    unsigned long long l;
    struct stat sb;
    off_t off, lim;
    char *eptr;
    void *ptr;

    if (a != 3) {
        fprintf(stderr, "usage: %s file/dev {-B|byte-pattern}\n", v[0]);
        exit(1);
    }
    ret = posix_memalign(&ptr, sysconf(_SC_PAGESIZE), MYBLK);
    if (ret) {
        fprintf(stderr, "failed to allocate memory\n");
        exit(1);
    }
    fd = open(v[1], O_RDWR|O_APPEND||O_DIRECT);
    if (fd < 0) {
        perror(v[1]);
        exit(1);
    }
    if (fstat(fd, &sb) < 0) {
        perror(v[1]);
        exit(1);
    }
    if (S_ISREG(sb.st_mode)) {
        lim = sb.st_size;
        lseek(fd, 0, 0);
    } else if (S_ISBLK(sb.st_mode)) {
        if (ioctl(fd, BGS64, &val) < 0) {
            perror("BLKGETSIZE64");
            exit(1);
        }
        lim = (off_t) val;
    } else {
        fprintf(stderr, "cannot figure out limit\n");
        exit(1);
    }
    if (strcmp(v[2], "-B") != 0) {
        l = strtoull(v[2], &eptr, 0);
        if (eptr == v[2]) {
            fprintf(stderr, "bad numeric arg %s\n", v[2]);
            exit(1);
        }
        if (l < 256) {
            unsigned char x = l;
            for (i = 0; i < MYBLK; i++)
                ((unsigned char *)ptr)[i] = x;
        } else if (l < 65536) {
            unsigned short x = l;
            for (i = 0; i < (MYBLK >> 1); i++)
                ((unsigned short *)ptr)[i] = x;
        } else if (l < (1ULL << 32)) {
            unsigned int x = l;
            for (i = 0; i < (MYBLK >> 2); i++)
                ((unsigned int *)ptr)[i] = x;
        } else {
            for (i = 0; i < (MYBLK >> 3); i++)
                ((unsigned long long *)ptr)[i] = l;
        }
    }
    for (w = off = 0; off < lim; off += MYBLK) {
        if (strcmp(v[2], "-B") == 0) {
            for (i = 0; i < (MYBLK >> 2); i++) {
                ((unsigned int *)ptr)[i] = ((off + (i << 2)) >> 9);
            }
        }
        w = write(fd, ptr, MYBLK);
        if (w != MYBLK)
            break;
    }
    if (w < 0) {
        perror("write");
    }
    exit(0);
}
/*
 * vim:ts=4:sw=4:expandtab
 */

