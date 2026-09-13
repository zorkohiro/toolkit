/*
#!/bin/bash
DATA=/var/lib/temper/data
TMP2=$(mktemp /tmp/tcvt_b_XXXXXX)
TMP3=$(mktemp /tmp/tcvt_c_XXXXXX)

function doexit {
 rm -f $TMP2 $TMP3
 exit $1
}

cat $DATA | while read d c f; do ld=$(date --date="@${d}" +"%b %e %Y %H:%M"); echo $ld $f; done > $TMP2

cat > $TMP3<<EOF
set terminal png truecolor size 1536, 768 font "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf"
set autoscale
set xdata time
set xtics rotate font "/usr/share/fonts/truetype/liberation/LiberationSans-Italic.ttf,9"
set timefmt "%b %d %Y %H:%M"
set format x "%h %d %Y"
set output "/var/www/feral.com/public_html/temp/temp.png"
set xlabel "Date" font "/usr/share/fonts/truetype/liberation/LiberationSans-Italic.ttf,8"
EOF
echo "plot \"$TMP2\"  using 1:5 t \"Temperature Degrees F\"" >> $TMP3
gnuplot $TMP3
doexit $?
*/

#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <inttypes.h>
#include <time.h>
#include <string.h>
#include <fcntl.h>

#define TCVT    "/tmp/tcvt.tmp"
#define PLOT    "/tmp/tcvt.plot"

int
main(int a, char **v)
{
    FILE *wp, *ip;
    unsigned long t, line = 0;
    char buffer[32], f[8];
    int debug = 0;

    if (a != 3) {
        fprintf(stderr, "usage: %s inputfile outputfile\n", v[0]);
        exit(1);
    }
    ip = fopen(v[1], "r");
    if (ip == NULL) {
        perror(v[1]);
        exit(1);
    }

    wp = fopen(TCVT, "w");
    if (wp == NULL) {
        perror(TCVT);
        exit(1);
    }

    if (getenv("DEBUG") != NULL)
        debug++;

    while (fscanf(ip, "%lu %s %s\n", &t, buffer, f) == 3) {
        struct tm tmval;
        time_t tval = t;

        line++;

        if (localtime_r(&tval, &tmval) == NULL) {
            fprintf(stderr, "bad localtime at line %lu (%s)\n", line, strerror(errno));
            exit(1);
        }

        if (strftime(buffer, sizeof (buffer), "%b %e %Y %H:%M", &tmval) == 0) {
            fprintf(stderr, "bad strftime conversion at line %lu\n", line);
            exit(1);
        }
        if (fprintf(wp, "%s %s\n", buffer, f) < 0) {
            perror("writing output");
            exit(1);
        }
    }
    if (ferror(ip)) {
        perror("fscanf");
        exit(1);
    }
    fclose(ip);
    fclose(wp);
    if (debug)
        fprintf(stderr, "read %lu lines\n", line);
    wp = fopen(PLOT, "w");
    if (wp == NULL) {
        perror(PLOT);
        exit(1);
    }
    fprintf(wp, "set terminal png truecolor size 1536, 768 font \"/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf\"\n");
    fprintf(wp, "set autoscale\n");
    fprintf(wp, "set xdata time\n");
    fprintf(wp, "set xtics rotate font \"/usr/share/fonts/truetype/liberation/LiberationSans-Italic.ttf,9\"\n");
    fprintf(wp, "set timefmt \"%%b %%d %%Y %%H:%%M\"\n");
    fprintf(wp, "set format x \"%%h %%d %%Y\"\n");
    fprintf(wp, "set output \"%s\"\n", v[2]);
    fprintf(wp, "set xlabel \"Date\" font \"/usr/share/fonts/truetype/liberation/LiberationSans-Italic.ttf,8\"\n");
    fprintf(wp, "plot \"%s\"  using 1:5 t \"Temperature Degrees F\"\n", TCVT);
    fclose(wp);
    sprintf(buffer, "gnuplot %s", PLOT);
    int r = 0;
    if (system(buffer)) {
        fprintf(stderr, "failure to run gnuplot: %s\n", strerror(errno));
        r = 1;
    }
    unlink(PLOT);
    unlink(TCVT);
    exit (r);
}
