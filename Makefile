FLAGS = -Wall -Werror -fPIC

export LD_LIBRARY_PATH=$(CURDIR):$$(LD_LIBRARY_PATH)

.PHONY: lwp

all: lwp

lwp: liblwp.a

lwp.o: lwp.c
	gcc $(FLAGS) -c -o lwp.o lwp.c

liblwp.a: lwp.o
	ar r liblwp.a lwp.o
	ranlib liblwp.a

clean:
	rm -rf lwp.o liblwp.a *~ TAGS core