/*
 * Copyright (c) 2015, 2021 by Matthew Jacob
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
// Transform a tree containing a mix of mp4 and mp3 files
// and other items into just mp3 files in a new directory
#define _XOPEN_SOURCE 500
#include <ftw.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <fcntl.h>
#include <sys/wait.h>

static char *odir;

static int
work_fn(const char *path, const struct stat *sb, int tflag, struct FTW *ftwbuf)
{
    char buf[1024];
    char fub[1024];
    char *args[12];
    pid_t pid;
    int status;
    enum { MP3, M4A } typ;
    char *p;
    size_t len = 0;

    if (tflag != FTW_F)
        return (0);
    strcpy(buf, path);
    if (strncmp(buf, "./", 2) == 0) {
        strcpy(fub, buf+2);
        strcpy(buf, fub);
    }
    p = &buf[strlen(buf) - 3];
    if (strcmp(p, "mp3") == 0)
        typ = MP3;
    else if (strcmp(p, "m4a") == 0)
        typ = M4A;
    else
        return (0);
    printf("%s\n", buf);
    len = snprintf(fub, sizeof (fub), "%s/%s", odir, buf);
    p = strrchr(fub, '/');
    if (p) {
        *p = 0;
        args[0] = "mkdir";
        args[1] = "-p";
        args[2] = fub;
        args[3] = 0;
        pid = fork();
        if (pid < 0) {
            perror("fork");
            return(1);
        }
        if (pid == 0) {
            execvp("mkdir", args);
            exit(1);
        }
        if (waitpid(pid, &status, 0) < 0) {
            perror("wait");
            return(1);
        }
        if (WEXITSTATUS(status) != 0) {
            printf("cannot mkdir %s\n", fub);
            return(1);
        }
        *p = '/';
    }
    if (typ == MP3) {
        int fdi = open(buf, O_RDONLY);
        int fdo = open(fub, O_WRONLY|O_CREAT|O_TRUNC, 0644);
        size_t r, w;
        char xfb[8192];

        if (fdi < 0) {
            perror(buf);
            return (1);
        }
        if (fdo < 0) {
            perror(fub);
            return (1);
        }
        while ((r = read(fdi, xfb, sizeof (xfb))) > 0) {
            w = write(fdo, xfb, r);
            if (w != r) {
                perror(fub);
                return (1);
            }
        }
        if (r < 0) {
            perror(buf);
            return (1);
        }
        (void) close(fdi);
        (void) close(fdo);
        return (0);
    }
    fub[len - 2] = 'p';
    fub[len - 1] = '3';

    args[0] = "ffmpeg";
    args[1] = "-v";
    args[2] = "0";
    args[3] = "-ab";
    args[4] = "131072";
    args[5] = "-ac";
    args[6] = "2";
    args[7] = "-i";
    args[8] = buf;
    args[9] = fub;
    args[10] = 0;

    pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(1);
    }
    if (pid == 0) {
        int fd;
        close(1);
        fd = open("/tmp/xform_m4a_log", O_WRONLY|O_CREAT|O_TRUNC, 0755);
        if (fd < 0) {
            perror("failed to create /tmp/xform_m4a_log");
            exit(1);
        }
        close(2);
        fd = dup(fd);
        if (fd < 0) {
            printf("failed to dup stdin\n");
            exit(1);
        }
        execvp("ffmpeg", args);
        exit(1);
    }
    if (waitpid(pid, &status, 0) < 0) {
        perror("wait");
        return(1);
    }
    if (WEXITSTATUS(status) != 0) {
        printf("failed to transform %s\n", buf);
        return(1);
    }
    return (0);
}

int
main(int a, char **v)
{
    if (a != 3) {
        fprintf(stderr, "usage: %s sourcedir destdir\n", v[0]);
        return (1);
    }

    if (chdir(v[1]) < 0) {
        perror(v[1]);
        exit(1);
    }
    odir = v[2];
    if (nftw(".", work_fn, 20, 0) < 0) {
        printf("did not complete\n");
        return (1);
    }
    return (0);
}
/*
 * vim:ts=4:sw=4:expandtab
 */
