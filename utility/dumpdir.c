/* Copyright (c) 1979 Regents of the University of California */
/*
 * Additional Copyright (c) 2008, 2009 by Matthew Jacob
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
 * dump the current directory
 */
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>

static const char *tweak_sz(off_t ival, unsigned int *man);

int
main(void)
{
    struct dirent *dp;
    struct stat sb;
    const char *exp;
    unsigned int man;
    DIR *dirp;

    dirp = opendir(".");
    if (dirp == NULL) {
        perror(".");
        exit(EXIT_FAILURE);
    }

    while ((dp = readdir(dirp)) != NULL) {
        if (dp->d_type != DT_REG)
            continue;

        if (strcmp(".", dp->d_name) == 0 || strcmp("..", dp->d_name) == 0)
            continue;

        if (stat(dp->d_name, &sb) < 0) {
            perror(dp->d_name);
            exit(EXIT_FAILURE);
        }
        exp = tweak_sz(sb.st_size, &man);
        printf("%6u%3s %6lu %s\n", man, exp, sb.st_nlink, dp->d_name);
    }
    exit(EXIT_SUCCESS);
}

static const char *
tweak_sz(off_t ival, unsigned int *man)
{
    const char *exp;
    unsigned long long val = ival;

    if (val < (1ULL  << 20)) {
        exp = "   ";
        *man = val;
    } else if (val < (1ULL << 30)) {
        exp = "MiB";
        *man = (val >> 20);
    } else if (val < (1ULL << 40)) {
        exp = "GiB";
        *man = (val >> 30);
    } else if (val < (1ULL << 50)) {
        exp = "TiB";
        *man = (val >> 40);
    } else if (val < (1ULL << 60)) {
        exp = "PiB";
        *man = (val >> 50);
    } else {
        exp = "XXX";
        *man = 0xffffffff;
    }
    return (exp);
}
/*
 * vim:ts=4:sw=4:expandtab
 */
