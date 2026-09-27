#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <memory.h>

static off_t
find_next_token(FILE *fp, char *token, int is_end_token)
{
    off_t pstrt = (off_t) -1;
    size_t tlen = strlen(token);
    char bbuf[256];
    int c;

    if (tlen > (sizeof(bbuf) - 1)) {
        fprintf(stderr, "token length too big (%zu)\n", tlen - 1);
        exit(1);
    }

    /* find the next '<' in the IO stream ahead */
    while ((c = fgetc(fp)) != EOF) {
        size_t i;
        if (c != '<') {
            continue;
        }
        pstrt = ftell(fp) - 1; /* remember the position of the starting '<' */

        /*
         * Are we looking for an end token?
         */
        int ie_off = 0;
        if (is_end_token) {
            c = fgetc(fp);
            if (c == EOF) {
                fprintf(stderr, "unexpected EOF at offset %jd looking for '/'\n", pstrt + 1);
                exit(1);
            }
            if (c != '/') {
                continue;
            }
            ie_off = 1;
        }
        /* read the length of the token if possible */
        for (i = 0; i < tlen; i++) {
            c = fgetc(fp);
            if (c == EOF) {
                fprintf(stderr, "unexpected EOF at offset %jd\n", pstrt + ie_off + i);
                exit(1);
            }
            bbuf[i] = c;
        }
        /* NULL terminate the buf */
        bbuf[i] = '\0';

        /* see if we have a match to the token read */
        int sr = strcmp(bbuf, token);
        if (sr == 0) {
            c = fgetc(fp);
            if (c == EOF) {
                fprintf(stderr, "unexpected EOF at token starting at offset %jd while checking for token closure\n", pstrt);
                exit(1);
            }
            if (c == '>') {
                /*
                 * If this is an end_token search, we return the offset of the closing '>'.
                 * If this is a beginning token search, return the offset of the initial '<'.
                 *
                 * The file offset is left at one past where we read the closure.
                 */
                if (is_end_token)
                    return(ftell(fp) - 1);
                else
                    return (pstrt);
            }
            /*
             * Return back to the offset of the first character after the initial '<'
             * (whether a start or end token).
             */
            if (fseek(fp, pstrt+1, SEEK_SET) < 0) {
                fprintf(stderr, "seek failure to %jd: %s\n", pstrt + 1, strerror(errno));
                exit(1);
            }
        }
    }
    return(pstrt);
}

int
main(int a, char **v)
{
    FILE *fp_in;
    off_t pos, npos, first_pos;

    if (a != 2) {
        fprintf(stderr, "usage: %s input-html\n", v[0]);
        exit(1);
    }
    fp_in = fopen(v[1], "r");
    if (fp_in == NULL) {
        fprintf(stderr, "unable to open %s: %s\n", v[1], strerror(errno));
        exit(1);
    }

    int state = 0;
    for (int fno = 0;;) {
        pos = find_next_token(fp_in, "tr", 0);
        if (pos < 0) {
            break;
        }
        if (state == 0) {
            first_pos = pos;
            state = 1;
            
        }
        npos = find_next_token(fp_in, "tr", 1);
        if (npos < 0) {
            break;
        }
        if ((npos - first_pos) > (10 << 20)) {
            printf("writing new split file starting at %jd and ending at %jd size %jd bytes\n",
                first_pos, npos, (npos - first_pos));
            fseek(fp_in, first_pos, SEEK_SET);
            char fname[32];
            snprintf(fname, sizeof (fname), "subpart_%04d.html", fno++);
            FILE *fp_o = fopen(fname, "w");
            if (fp_o == NULL) {
                perror(fname);
                exit(1);
            }
            FILE *hp_i = fopen("header", "r");
            for (int c = fgetc(hp_i); c != EOF; c = fgetc(hp_i)) {
                if (fputc(c, fp_o) == EOF) {
                    fprintf(stderr, "error writing to %s: %s\n", fname, strerror(errno));
                    exit(1);
                }
            }
            if (ferror(hp_i)) {
                fprintf(stderr, "error reading header file: %s", strerror(errno));
                exit(1);
            }
            fclose(hp_i);
            fseek(fp_in, first_pos, SEEK_SET);
            for (off_t nc = npos - first_pos; nc > 0; nc -= 1) {
                int c = fgetc(fp_in);
                if (c == EOF) {
                    fprintf(stderr, "failed to read a byte during transfer: %s\n", strerror(errno));
                    exit(1);
                }
                if (fputc(c, fp_o) == EOF) {
                    fprintf(stderr, "failed to write a byte during transfer: %s\n", strerror(errno));
                    exit(1);
                }
            }
            FILE *tp_i = fopen("trailer", "r");
            for (int c = fgetc(tp_i); c != EOF; c = fgetc(tp_i)) {
                if (fputc(c, fp_o) == EOF) {
                    fprintf(stderr, "error writing to %s: %s\n", fname, strerror(errno));
                    exit(1);
                }
            }
            if (ferror(tp_i)) {
                fprintf(stderr, "error reading trailer file: %s", strerror(errno));
                exit(1);
            }
            fclose(tp_i);
            fseek(fp_in, npos, SEEK_SET);
            state = 0;
        }
    }
}
