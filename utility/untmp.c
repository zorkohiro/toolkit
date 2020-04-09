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
 * Remove stuff in /tmp and $TMDIR owned by the invoker.
 * Skip files with a leading '.' in the name.
 */
/* Idea from a similar BSD (2.9) distro's program */
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

int
main(void)
{
    struct stat stbuf;
    struct dirent *dp;
    char **listp;
    size_t d, ndir;
    uid_t uid;
    DIR *dirp;

    if ((uid = getuid()) == 0 || geteuid() == 0) {
        fprintf(stderr, "I'm afraid I can't let you do that, Dave. You cannot run as root.\n");
        exit(EXIT_FAILURE);
    }

    /*
     * Build list of directories to traverse. Should be max of two.
     */
    if (getenv("TMPDIR") != NULL) {
        ndir = 2;
    } else {
        ndir = 1;
    }
    listp = calloc(ndir, sizeof (char *));
    if (listp == NULL) {
        perror("calloc");
        exit(EXIT_FAILURE);
    }
    listp[0] = "/tmp";
    if (ndir == 2) {
        listp[1] = getenv("TMPDIR");
        if (listp[1][0] != '/') {
            fprintf(stderr, "TMPDIR not an absolute path\n");
            exit(EXIT_FAILURE);
        }
    }

    for (d = 0; d < ndir; d++) {

        if (chdir(listp[d]) < 0) {
            fprintf(stderr, "chdir %s: %s\n", listp[d], strerror(errno));
            exit(EXIT_FAILURE);
        }

        dirp = opendir(listp[d]);
        if (dirp == NULL) {
            perror(listp[d]);
            exit(EXIT_FAILURE);
        }

        while ((dp = readdir(dirp)) != NULL) {
            if (dp->d_type != DT_REG)
                continue;

            if (dp->d_name[0] == '.')
                continue;

            if (stat(dp->d_name, &stbuf) < 0)
                continue;

            if (stbuf.st_uid != uid)
                continue;

            if (unlink(dp->d_name)) {
                fprintf(stderr, "%s/%s: %s\n", listp[d], dp->d_name, strerror(errno));
                continue;
            }

            printf("removed %s/%s\n", listp[d], dp->d_name);
        }
        closedir(dirp);
    }
    exit(EXIT_SUCCESS);
}
/*
 * vim:ts=4:sw=4:expandtab
 */
