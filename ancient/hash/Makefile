#
# $Id: Makefile,v 1.2 2017/06/03 21:21:01 mjacob Exp $ 
#
#


CFLAGS	=	-Wall -O2 -DLARGEFILE64_SOURCE=1 -D_FILE_OFFSET_BITS=64
ALL	=	shabucket
LDLIBS +=	-lssl -lcrypto

all:	$(ALL)

clean:
	$(RM) $(ALL)
