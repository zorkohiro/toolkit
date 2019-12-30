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
 * Remove stuff in /tmp owned by the invoker.
 */
/* From an old BSD (2.9) distro */
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <dirent.h>
#include <fcntl.h>

int
main(void)
{
	struct stat stbuf;
	struct dirent *dp;
    uid_t uid;
    DIR *dirp;

	if ((uid = getuid()) == 0) {
		exit(EXIT_SUCCESS);
	}
    dirp = opendir("/tmp");
	if (dirp == NULL) {
		perror("/tmp");
		exit(EXIT_FAILURE);
	}
    if (chdir("/tmp") < 0) {
        perror("chdir /tmp");
        exit(EXIT_FAILURE);
    }
	
    while ((dp = readdir(dirp)) != NULL) {
        if (dp->d_type != DT_REG)
            continue;

        if (stat(dp->d_name, &stbuf) < 0)
            continue;

        if (stbuf.st_uid != uid) {
            continue;
        }
        if (unlink(dp->d_name)) {
            perror(dp->d_name);
            continue;
        }
        printf("%s\n", dp->d_name);
	}
	exit(EXIT_SUCCESS);
}
/*
 * vim:ts=4:sw=4:expandtab
 */
