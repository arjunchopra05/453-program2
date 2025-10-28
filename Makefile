FLAGS = -Wall -Werror -fPIC

export LD_LIBRARY_PATH=$(CURDIR):$$(LD_LIBRARY_PATH)

.PHONY: lwp libsmalloc.a

all: lwp

lwp: liblwp.a

lwp.o: lwp.c libsmalloc.a
	gcc $(FLAGS) -S -c -o lwp.o lwp.c libsmalloc.a

liblwp.a: lwp.o
	ar r liblwp.a lwp.o
	ranlib liblwp.a

clean:
	rm -rf lwp.o liblwp.a *~ TAGS core

smalloc.o: smartalloc.c
	gcc -Wall -Werror -fPIC -c -o smalloc.o smartalloc.c

libsmalloc.a: smalloc.o
	ar r libsmalloc.a smalloc.o
	ranlib libsmalloc.a

numbers: liblwp.a libsmalloc.a
	gcc -o numbers numbersmain.c liblwp.a libsmalloc.a