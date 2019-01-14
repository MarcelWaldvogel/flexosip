LIBS	= -leXosip2 -losip2 -losipparser2 -lcares -lsndfile -lortp -lm ${INILIB}
CFLAGS	= -g -I/usr/local/include ${INIINC} -Wall -Wextra -O -DENABLE_TRACE
LDFLAGS	= -g -L/usr/local/lib -Xlinker -rpath=/usr/local/lib
OFILES  = flexosip.o flexosnd.o flexortp.o

.PHONY: all clean
all:	demo flexosip.a

demo:	demo.o flexosip.a
	${CC} ${LDFLAGS} -o $@ $^ ${LIBS}

demo.o ${OFILES}: flexosip.h flexosnd.h flexortp.h unused.h

flexosip.a: ${OFILES}
	${AR} r $@ $^

clean:
	${RM} *.o *.a

# Decide between system-installed or local inih
# This is at the bottom to not make any of this the default target
ifeq ($(wildcard inih/ini.[c]), inih/ini.c)
INILIB  = # Will be included through the dependency; inih/ini.o
INIINC  = -Iinih

demo:	inih/ini.o
demo.o:	inih/ini.h
inih/ini.o: inih/ini.c inih/ini.h
else
INILIB  = -linih
endif
