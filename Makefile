FLAGS = -Wall -Werror -fPIC

.PHONY: lwp clean

all: lwp

lwp: liblwp.a

lwp.o: lwp.c
	gcc $(FLAGS) -c -o lwp.o lwp.c

liblwp.a: lwp.o smartalloc.o magic64.o
	ar rcs liblwp.a lwp.o magic64.o smartalloc.o
	ranlib liblwp.a

clean:
	rm -rf lwp.o liblwp.a magic64.o smartalloc.o *~ TAGS core

numbers: numbersmain.c liblwp.a
	gcc -Wall -Werror -o numbers numbersmain.c liblwp.a AlwaysZero.o

snakes: hungrysnakes.c liblwp.a
	gcc -Wall -Werror -o snakes hungrysnakes.c liblwp.a -lncurses AlwaysZero.o

cleantest:
	rm -rf numbers snakes

magic64.o: magic64.S
	gcc $(FLAGS) -c -o magic64.o magic64.S

smartalloc.o: smartalloc.c smartalloc.h
	gcc $(FLAGS) -c -o smartalloc.o smartalloc.c