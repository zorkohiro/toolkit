/* $Id: tape_test.c,v 1.4 2007/09/22 04:26:40 mjacob Exp $ */
/*
 * Copyright (c) 1997 by Feral Software
 */
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <memory.h>
#include <sys/ioctl.h>
#include <sys/mtio.h>
#include <sys/time.h>

#if	defined(sun) && defined(__svr4__)
typedef unsigned long long loctype;
#else
typedef unsigned int loctype;
#endif

typedef struct {
    /*
     * Size of this block, in bytes.
     */
    unsigned int blksize;

    /*
     * Location, as known via file number && record in file.
     */

    daddr_t fileno;
    daddr_t recno;

    /*
     * Hardware Block Location
     */

    unsigned int hblock;

    /*
     * SCSI Logical block location.
     */
    unsigned int sblock;

} tape_block;



#ifndef	MTIOCRDHPOS
#define	MTIOCRDHPOS	-1
#define	MTIOCRDSPOS	-1
#define	MTIOCSLOCATE	-1
#define	MTIOCHLOCATE	-1
#endif

static int space_to(int, tape_block *, tape_block *);
static int locate_to(int, int, tape_block *);
static void mtstat(char *, int);
static int get_block_pos(int, tape_block *);
static int dmiscompr(unsigned int *, tape_block *, const char *);
static uint64_t szarg(char *n);

static char *dname = NULL;
static int nohloc = 0;
#if	defined(__NetBSD__)
static int nosloc = 1;
#else
static int nosloc = 0;
#endif
static int udb = 0;

#define	LRECSIZE	(recsize / sizeof (unsigned int))
#define	NTBS		((nrecperfile * nfiles) + 1)
#define	PRINT_SETUP	\
    if (nrecperfile > 1000) \
	printevery = 250; \
    else if (nrecperfile > 100) \
	printevery = 25; \
    else if (nrecperfile > 10) \
	printevery = 7; \
    else \
	printevery = 1

#define	PRINT_POS	\
    if (--printevery <= 0) { \
	if (nohloc) \
	    fprintf(stdout, "File %8d Rec %8d\n", tbp->fileno, tbp->recno); \
	else if (udb) \
	    fprintf(stdout, "File %8d Rec %8d Device Block Location %d\n", \
		tbp->fileno, tbp->recno, tbp->hblock); \
	else \
	    fprintf(stdout, "File %8d Rec %8d SCSI Block Location %d\n", \
		tbp->fileno, tbp->recno, tbp->sblock); \
	PRINT_SETUP; \
    }

/*
 * The actual position is the last record we had I/O
 * to/from, but adjusted for the I/O done (i.e., the
 * record count gets bumped by 1).
 */
#define	ADDRESS_AFTER_IO(lptr, nptr)	*(nptr) = *(lptr), (nptr)->recno++

int
main(int a, char **v)
{
    long nfiles, nkbytes, recsize, nkbperfile, nrecperfile;
    tape_block *tbs, *tbp, *lasttbp;
    unsigned int *dbuf;
    struct mtop op;
    int i, file, rec, w, r, fd, printevery = 0, oneshot = 1, check = 0;
    struct timeval stime, etime;

    if (a != 7 && a != 8) {
	printf("usage: %s [-c] {hard|logical|none} norewdev #files "
	       "#total-mb #recsiz {once|continuous}\n", v[0]);
	return (1);
    }
    srandom(getpid());

    if (strcmp(*++v, "-c") == 0) {
	check = 1;
    } else {
	--v;
    }

    if (strcmp(*++v, "hard") == 0) {
	udb = 1;
    } else if (strcmp(*v, "none") == 0) {
	nohloc = 1;
    }

    dname = *++v;	/* record for later usage */

    nfiles = atol(*++v);
    if (nfiles < 2) {
	fprintf(stderr, "I need at least 2 files to work with\n");
	return (1);
    }
    nkbytes = atol(*++v) << 10;
    recsize = szarg(*++v);
    if (nfiles <= 0 || nkbytes <= 0 || recsize <= 0) {
	fprintf(stderr, "Get real.\n");
	return (1);
    }
    nkbperfile = nkbytes / nfiles;
    if (recsize > (nkbperfile << 10)) {
	fprintf(stderr, "record size is greater than size of a tape file\n");
	return (1);
    }
    nrecperfile = (nkbperfile << 10) / recsize;
    if (nrecperfile < 2) {
	fprintf(stderr, "I need at least 2 records per tape file\n");
	return (1);
    }

    if (strcmp(*++v, "once") != 0) {
	oneshot = 0;
    }


    dbuf = (unsigned int *) malloc(recsize);
    if (dbuf == NULL) {
	perror("malloc");
	return (1);
    }

    tbp = tbs = (tape_block *) malloc(NTBS * sizeof (tape_block));
    if (tbp == NULL) {
	perror("malloc");
	return (1);
    }
    (void) memset((void *) tbs, 0, NTBS * sizeof (tape_block));

    fd = open(dname, O_RDWR);
    if (fd < 0) {
	perror(dname);
	return (1);
    }

    /*
     * Rewind the tape
     */
    op.mt_op = MTREW;
    op.mt_count  = 1;
    if (ioctl(fd, MTIOCTOP, (caddr_t) &op) < 0) {
	fprintf(stderr, "%s: first rewind failed (%d)\n", dname, errno);
	return (1);
    }

    /*
     * TODO: Get current device status to figure out whether it's
     * in fixed record mode or not- if such, override the specified
     * values.
     */

    /*
     * TODO: Add argument to allow for varying record sizes.
     */

    /*
     * Get the initial block position after a rewind.
     */
    if (nohloc == 0) {
	if (get_block_pos(fd, tbp)) {
	    nohloc = 1;
	} else if (udb == 0 && tbp->sblock != 0) {
	    fprintf(stderr, "Nonzero BOT (%d) block location-\n", tbp->sblock);
	    fprintf(stderr, "This will not work for logical "
		    "block locating.\n");
	    return (1);
	}
    }

    if (nohloc) {
	(void) memset((void *)tbp, 0, sizeof (*tbp));
	tbp->sblock = (unsigned int) -1;
	tbp->hblock = (unsigned int) -1;
    }

    /*
     * Write tape, with all records having different data.
     */
    printf("Writing %d files of %d, %d byte, records each\n", nfiles,
	   nrecperfile, recsize);

    for (file = 0; file < nfiles; file++) {
	for (rec = 0; rec < nrecperfile; rec++) {
	    tbp->blksize = recsize;
	    tbp->fileno = file;
	    tbp->recno = rec;
	    if (nohloc) {
		tbp->sblock = (unsigned int) -1;
		tbp->hblock = (unsigned int) -1;
	    } else {
		if (get_block_pos(fd, tbp)) {
		    mtstat(dname, fd);
		    return (1);
		}
	    }

	    for (i = 0; i < (tbp->blksize / (sizeof (int))); i += 4) {
		if (nohloc) {
		    dbuf[i+0] = tbp->fileno;
		    dbuf[i+1] = tbp->recno;
		} else if (udb) {
		    dbuf[i+0] = tbp->hblock;
		    dbuf[i+1] = -tbp->hblock;
		} else {
		    dbuf[i+0] = tbp->sblock;
		    dbuf[i+1] = -tbp->sblock;
		}
		dbuf[i+2] = getpid();
		dbuf[i+3] = i;
	    }

	    PRINT_POS;

	    w = write(fd, dbuf, tbp->blksize);
	    if (w < 0) {
		perror("write");
		mtstat(dname, fd);
		return (1);
	    } else if (w != tbp->blksize) {
		fprintf(stderr, "Short write @ file %d rec %d (%d)\n",
			file, rec, w);
		mtstat(dname, fd);
		i = -1;
		break;
	    }
	    tbp++;
	}
	if (i < 0)
	    break;
	op.mt_op = MTWEOF;
	op.mt_count = 1;
	if (ioctl(fd, MTIOCTOP, (caddr_t) &op) < 0) {
	    perror("writing filemark");
	    mtstat(dname, fd);
	    return (1);
	}
    }
    /*
     * To make sure that the last position marker makes sense,
     * i.e., we're positioned right after the last record
     * we wrote, we back up over the last filemark we wrote.
     */
    op.mt_op = MTBSF;
    op.mt_count = 1;
    if (ioctl(fd, MTIOCTOP, (caddr_t) &op) < 0) {
	perror("backspacing over last filemark");
	mtstat(dname, fd);
	return (1);
    }

    putchar('\n');

	
    nfiles = (tbp - tbs);
    lasttbp = tbp - 1;
    tbp = tbs;

    /*
     * If asked to, check each record we wrote by
     * moving to it and checking the data.
     */

    if (check) {
	tape_block adj, *t = tbs, *l = lasttbp;
	while (t != lasttbp) {

	    ADDRESS_AFTER_IO(l, &adj);

	    printf("checking file %d.%d..", t->fileno, t->recno);
	    if (nohloc) {
		if (space_to(fd, l, t)) {
		    return (1);
		}
	    } else {
		printf(".%s block location %d..",
		    udb? "hard" : "logical", udb? t->hblock : t->sblock);
		if (locate_to(fd, udb, t)) {
		    return (1);
		}
	    }
	    putchar('\n');
	    r = read(fd, dbuf, t->blksize);
	    if (r < 0) {
		perror("read after check positioning");
		mtstat(dname, fd);
		return (1);
	    } else if (r != t->blksize) {
		fprintf(stderr, "read after check returned %d (s.b.:%d)\n",
			r, t->blksize);
		return (1);
	    }
	    if (dmiscompr(dbuf, t, "check")) {
		return (1);
	    }
	    l = t;
	    t++;
	}
    }

    /*
     * Now test fsf/fsr && locating to the last record
     * in the tape files. If we're in continuous loop
     * mode, do this randomly forever.
     */

    do {
	tape_block adj;

	ADDRESS_AFTER_IO(lasttbp, &adj);

	/*
	 * Time spacing from current location to specified location.
	 */
	(void) gettimeofday(&stime, NULL);
	if (space_to(fd, &adj, tbp)) {
	    return (1);
	}
	(void) gettimeofday(&etime, NULL);
	r = read(fd, dbuf, tbp->blksize);
	if (r < 0) {
	    perror("read after fsf/fsr positioning");
	    mtstat(dname, fd);
	    return (1);
	} else if (r != tbp->blksize) {
	    fprintf(stderr, "read after fsf/fsr returned %d (s.b.:%d)\n",
		    r, tbp->blksize);
	    return (1);
	}
	if (dmiscompr(dbuf, tbp, "fsf/fsr")) {
	    return (1);
	}

	if (etime.tv_usec < stime.tv_usec) {
	    etime.tv_usec += 1000000;
	    etime.tv_sec--;
	}
	etime.tv_usec -= stime.tv_usec;
	etime.tv_usec /= 100000;
	etime.tv_sec -= stime.tv_sec;
	printf("%s Seek to {%d,%d} from {%d,%d} took %d.%d secs\n",
	    (nosloc)? "Rewind/FSF/FSR": "FSF/FSR", tbp->fileno, tbp->recno,
	    adj.fileno, adj.recno, etime.tv_sec, etime.tv_usec);

	if (nohloc == 0) {
	    /*
	     * Return to original position- using hardware location
	     * along with FSR to get there, (but not timing it).
	     */
	    if (locate_to(fd, udb, lasttbp)) {
		return (1);
	    }
	    op.mt_op = MTFSR;
	    op.mt_count = 1;
	    if (ioctl(fd, MTIOCTOP, (caddr_t) &op) < 0) {
		perror("record space after locate");
		mtstat(dname, fd);
		return (1);
	    }

	    /*
	     * Now *locate* to desired block
	     */
	    (void) gettimeofday(&stime, NULL);
	    if (locate_to(fd, udb, tbp)) {
		return (1);
	    }
	    (void) gettimeofday(&etime, NULL);
	    r = read(fd, dbuf, tbp->blksize);
	    if (r < 0) {
		perror("read after locate");
		mtstat(dname, fd);
		return (1);
	    } else if (r != tbp->blksize) {
		fprintf(stderr, "read after locate returned %d (s.b.:%d)\n",
			r, tbp->blksize);
		return (1);
	    }
	    if (dmiscompr(dbuf, tbp, "locate")) {
		return (1);
	    }
	    if (etime.tv_usec < stime.tv_usec) {
		etime.tv_usec += 1000000;
		etime.tv_sec--;
	    }
	    etime.tv_usec -= stime.tv_usec;
	    etime.tv_usec /= 100000;
	    etime.tv_sec -= stime.tv_sec;
	    printf("Locate  Seek to %s Location %d took %d.%d secs\n", udb?
		   "Device Block" : "SCSI Logical Block",
		   udb? tbp->hblock : tbp->sblock,
		   etime.tv_sec, etime.tv_usec);
	}
	putchar('\n');
	file = random() % nfiles;
	if (file >= nfiles)
	    file -= 1;
	lasttbp = tbp;
	tbp = tbs + file;
    } while (oneshot == 0);
    return (0);
}


static int
space_to(int fd, tape_block *curloc, tape_block *newloc)
{
    typedef enum { ReW, FsF, FsR, BsF, BsR } tpop;
    struct {
	tpop op;
	int cnt;
    } tape_ops[4];
    int opcnt, curop;
    struct mtop op;
    struct mtget mt;
    tape_block src = *curloc;

#if	0
    printf("space_to: %d.%d from %d.%d\n", newloc->fileno, newloc->recno,
	   curloc->fileno, curloc->recno);
#endif

    if (nohloc) {
	if (nosloc) {
	    op.mt_op = MTREW;
	    op.mt_count = 1;
	    if (ioctl(fd, MTIOCTOP, (caddr_t) &op) < 0) {
		fprintf(stderr, "%s: rewind failed (%d)\n", dname, errno);
		return (-1);
	    }
	    src.fileno = 0;
	    src.recno = 0;
	} else {
	    if (ioctl(fd, MTIOCGET, (caddr_t)&mt) < 0) {
		perror("MTIOCGET");
		return (-1);
	    }
	    if (mt.mt_blkno != 1000000000 && (mt.mt_fileno != curloc->fileno ||
		mt.mt_blkno != curloc->recno)) {
		fprintf(stderr, "space_to: curloc has %d.%d, IOCGET has"
			" %d.%d\n", curloc->fileno, curloc->recno, mt.mt_fileno,
			mt.mt_blkno);
	    }
	}
    }

    if (newloc->fileno == curloc->fileno && newloc->recno == curloc->recno) {
	return (0);
    }

    opcnt = 0;

    /*
     * Are we moving within the same tape file?
     * If not, then get to the beginning of the target tape file.
     */

    if (newloc->fileno != src.fileno) {
	if (newloc->fileno == 0) {
	    tape_ops[opcnt].op = ReW;
	    tape_ops[opcnt++].cnt = 1;
	} else if (newloc->fileno > src.fileno) {
	    tape_ops[opcnt].op = FsF;
	    tape_ops[opcnt++].cnt = newloc->fileno - src.fileno;
	} else {
	    tape_ops[opcnt].op = BsF;
	    tape_ops[opcnt++].cnt = src.fileno - newloc->fileno + 1;
	    tape_ops[opcnt].op = FsF;
	    tape_ops[opcnt++].cnt = 1;
	}
	if (newloc->recno > 0) {
	    tape_ops[opcnt].op = FsR;
	    tape_ops[opcnt++].cnt = newloc->recno;
	}
    } else {
	if (newloc->recno > src.recno) {
	    tape_ops[opcnt].op = FsR;
	    tape_ops[opcnt++].cnt = newloc->recno - src.recno;
	} else {
	    tape_ops[opcnt].op = BsR;
	    tape_ops[opcnt++].cnt = src.recno - newloc->recno;
	}
    }

    for (curop = 0; curop < opcnt; curop++) {
	char *opname;
	switch(tape_ops[curop].op) {
	case ReW:
	    opname = "rewind";
	    op.mt_op = MTREW;
	    break;
	case FsF:
	    opname = "fsf";
	    op.mt_op = MTFSF;
	    break;
	case FsR:
	    opname = "fsr";
	    op.mt_op = MTFSR;
	    break;
	case BsF:
	    opname = "bsf";
	    op.mt_op = MTBSF;
	    break;
	case BsR:
	    opname = "bsr";
	    op.mt_op = MTBSR;
	    break;
	default:
	    fprintf(stderr, "tape op stack botch\n");
	    return (-1);
	}
	op.mt_count = tape_ops[curop].cnt;
	if (ioctl(fd, MTIOCTOP, (caddr_t) &op) < 0) {
	    perror(opname);
	    fprintf(stderr, "%s %d error(%d): curloc=%d,%d newloc=%d,%d\n",
		    opname, op.mt_count, errno, src.fileno,
		    src.recno, newloc->fileno, newloc->recno);
	    mtstat(dname, fd);
	    return (-1);
	}
    }
    if (nohloc && nosloc == 0) {
	if (ioctl(fd, MTIOCGET, (caddr_t)&mt) < 0) {
	    perror("MTIOCGET");
	    return (-1);
	}
	if (mt.mt_fileno != newloc->fileno || mt.mt_blkno != newloc->recno) {
	    fprintf(stderr, "space_to: newloc has %d.%d, IOCGET has %d.%d\n",
		    newloc->fileno, newloc->recno, mt.mt_fileno,
		    mt.mt_blkno);
	}
    }
    return (0);
}

static int
locate_to(int fd, int udb, tape_block *newloc)
{
    loctype val;
    if (udb) {
	val = (loctype) newloc->hblock;
	if (ioctl(fd, MTIOCHLOCATE, (caddr_t) &val) < 0) {
	    perror("MTIOCHLOCATE");
	    mtstat(dname, fd);
	    return (-1);
	}
    } else {
	val = (loctype) newloc->sblock;
	if (ioctl(fd, MTIOCSLOCATE, (caddr_t) &val) < 0) {
	    perror("MTIOCSLOCATE");
	    mtstat(dname, fd);
	    return (-1);
	}
    }
    return (0);
}


#ifdef	sun
static const char *sun_skeys[] = {
    "No Additional Sense",	/* 0x00 */
    "Soft Error",		/* 0x01 */
    "Not Ready",		/* 0x02 */
    "Media Error",		/* 0x03 */
    "Hardware Error",		/* 0x04 */
    "Illegal Request",		/* 0x05 */
    "Unit Attention",		/* 0x06 */
    "Write Protected",		/* 0x07 */
    "Blank Check",		/* 0x08 */
    "Vendor Unique",		/* 0x09 */
    "Copy Aborted",		/* 0x0a */
    "Aborted Command",		/* 0x0b */
    "Equal Error",		/* 0x0c */
    "Volume Overflow",		/* 0x0d */
    "Miscompare Error",		/* 0x0e */
    "Reserved",			/* 0x0f */
    "fatal",			/* 0x10 */
    "timeout",			/* 0x11 */
    "EOF",			/* 0x12 */
    "EOT",			/* 0x13 */
    "length error",		/* 0x14 */
    "BOT",			/* 0x15 */
    "wrong tape media",		/* 0x16 */
};
#endif

static void
mtstat(char *s, int fd)
{
    struct mtget mt;

    if (ioctl(fd, MTIOCGET, (caddr_t)&mt) < 0) {
	perror("MTIOCGET");
	return;
    }
#ifdef	sun
    fprintf(stderr, "%s: Sense key(0x%x)= %s   residual= %ld   ",
	    s, mt.mt_erreg, (mt.mt_erreg >= 0 && mt.mt_erreg <= 0x16)?
	    sun_skeys[mt.mt_erreg]: "????", mt.mt_resid);
#else
    fprintf(stderr, "%s: Error Reg= %x residual= %ld    ",
            s, mt.mt_erreg, mt.mt_resid);
#endif
    fprintf(stderr, "retries= %d\n", mt.mt_dsreg);
    if (nosloc == 0)
	fprintf(stderr, "%* s  File no= %d   block no= %d\n", strlen(s), " ",
	   mt.mt_fileno, mt.mt_blkno);
}


static int
get_block_pos(int fd, tape_block *tbp)
{
    static int nordhpos, nordspos;
    int rv = -1;
    loctype location;

    if (nordhpos) {
	tbp->hblock = (unsigned int) -1;
    } else {
	if (ioctl(fd, MTIOCRDHPOS, (caddr_t) &location) < 0) {
	    perror("MTIOCRDHPOS");
	    nordhpos = 1;
	    tbp->hblock = (unsigned int) -1;
	} else {
	    rv = 0;
	    tbp->hblock = (unsigned int) location;
        }
    }
    if (nordspos) {
	tbp->sblock = (unsigned int) -1;
    } else {
	if (ioctl(fd, MTIOCRDSPOS, (caddr_t) &location) < 0) {
	    perror("MTIOCRDSPOS");
	    nordspos = 1;
	    tbp->sblock = (unsigned int) -1;
	} else {
	    rv = 0;
	    tbp->sblock = (unsigned int) location;
        }
    }
    return (rv);
}

static int
dmiscompr(unsigned int *dbuf, tape_block *tbp, const char *m)
{
    int i;
    int badmatch = 0;

    for (i = 0; i < tbp->blksize/sizeof (unsigned int); i += 4) {
	unsigned int sb[2];

	if (nohloc) {
	    if (dbuf[i+0] != (unsigned int) tbp->fileno ||
		dbuf[i+1] != (unsigned int) tbp->recno) {
		badmatch |= 1;
	    }
	    sb[0] = (unsigned int) tbp->fileno;
	    sb[1] = (unsigned int) tbp->recno;
	} else if (udb) {
	    if (dbuf[i+0] != (unsigned int) tbp->hblock  ||
		dbuf[i+1] != (unsigned int) -tbp->hblock) {
		badmatch |= 2;
	    }
	    sb[0] = (unsigned int) tbp->hblock;
	    sb[1] = (unsigned int) -tbp->hblock;
	} else {
	    if (dbuf[i+0] != (unsigned int) tbp->sblock ||
		dbuf[i+1] != (unsigned int) -tbp->sblock) {
		badmatch |= 4;
	    }
	    sb[0] = (unsigned int) tbp->sblock;
	    sb[1] = (unsigned int) -tbp->sblock;
	}
	if (dbuf[i+2] != (unsigned int) getpid() ||
	    dbuf[i+3] != (unsigned int) i) {
	    badmatch |= 8;
	}
	if (badmatch) {
	    fprintf(stderr, "%s: miscompared record (0x%x) at offset %d\n",
		m, badmatch, i);
	    fprintf(stderr, "\tFile %8d Rec %8d Hardware Location %d\n",
		    tbp->fileno, tbp->recno, (udb)? tbp->hblock : tbp->sblock);
	    fprintf(stderr, "\tdbuf[%d]=%d dbuf[%d]=%d dbuf[%d]=%d "
		    "dbuf[%d]=%d\n", i, dbuf[i+0], i+1, dbuf[i+1],
		    i + 2, dbuf[i+2], i + 3, dbuf[i+3]);
	    fprintf(stderr, "\n    Should Be:\n\n");
	    fprintf(stderr, "\tdbuf[%d]=%d dbuf[%d]=%d dbuf[%d]=%d "
		"dbuf[%d]=%d\n", i, sb[0], i+1, sb[1], i+2,
		getpid(), i+3, i);
	    break;
	}
    }
    return (badmatch);
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
 * mode: c
 * compile-command: "gcc -O -o locate_test locate_test.c"
 * Local variables:
 * c-indent-level: 4
 * c-brace-imaginary-offset: 0
 * c-brace-offset: -4
 * c-argdecl-indent: 4
 * c-label-offset: -4
 * c-continued-statement-offset: 4
 * c-continued-brace-offset: 0
 * End:
 */

