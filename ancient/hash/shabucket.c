/*
 * Copyright (c) 2017 by Matthew Jacob
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions, and the following disclaimer,
 *     without modification, immediately at the beginning of the file.
 *  2. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 *  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 *  ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 *  OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 *  OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 *  SUCH DAMAGE.
 */
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <openssl/sha.h>

#define	BUCKET_SIZE	(16 << 20)
static char buf[BUCKET_SIZE];

int
main(int a, char **v)
{
	SHA256_CTX context;
	unsigned char md[SHA256_DIGEST_LENGTH];
	ssize_t r;
	unsigned long chunk;
	char *file;
	int fd, i, fn;

	if (a < 2) {
		fprintf(stderr, "usage: shabucket file\n");
		exit(1);
	}

	for (fn = 1;  fn < a; fn++) {
		file = v[fn];

		fd = open(file, O_RDONLY);
		if (fd < 0) {
			perror(file);
			exit(1);
		}

		chunk = 0;
		while ((r = read(fd, buf, BUCKET_SIZE)) == BUCKET_SIZE) {
			if (!SHA256_Init(&context)) {
				fprintf(stderr, "unable to initialize context\n");
				exit (1);
			}

			if (!SHA256_Update(&context, (unsigned char*)buf, BUCKET_SIZE)) {
				fprintf(stderr, "unable to update context\n");
				exit (1);
			}

			if (!SHA256_Final(md, &context)) {
				fprintf(stderr, "unable to finalize context\n");
				exit (1);
			}

			fprintf(stdout, "%50s 16MB chunk %8lu ", file, chunk++);
			for (i = 0; i < SHA256_DIGEST_LENGTH; i++)
				fprintf(stdout, "%02x", md[i]);
			putchar('\n');
		}

		if (r < 0) {
			perror("read");
			exit(1);
		}
		(void) close(fd);
	}
	return (0);
}
