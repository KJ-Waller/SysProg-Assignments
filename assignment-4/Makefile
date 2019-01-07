###################### DEFS

CC = gcc
CFLAGS = -Wall -Werror
LDFLAGS = -ldl

###################### HELPERS

%.so : %.c
	$(CC) $(CFLAGS) -shared -nostdlib -fPIC $+ -o $@

%.o : %.c
	${CC} ${CFLAGS} -c $? -o $@

####################### TARGETS

# add your libraries to this line, as in 'libtest.so', make sure you have a 'libtest.c' as source
LIBS = libblank.so

.PHONY : all clean distclean

all : audioclient audioserver ${LIBS}

audioclient : audioclient.o audio.o
	${CC} ${CFLAGS} -o $@ $+

audioserver : audioserver.o audio.o
	${CC} ${CFLAGS} -o $@ $+

distclean : clean
	rm -f audioserver audioclient *.so
clean:
	rm -f $(OBJECTS) audioserver audioclient *.o *.so *~

