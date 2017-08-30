all: main

main: main.o lwt.o kalloc.o channel.o rbuf.o test_lmbench_compare.o
	gcc main.o lwt.o kalloc.o channel.o rbuf.o test_lmbench_compare.o -o main -O3
	
main.o: main.c
	gcc -c main.c -O3

lwt.o: lwt.c
	gcc -c lwt.c -O3

kalloc.o: kalloc.c
	gcc -c kalloc.c -O3

channel.o: channel.c
	gcc -c channel.c -O3

rbuf.o: rbuf.c
	gcc -c rbuf.c -O3

test_lmbench_compare.o: test_lmbench_compare.c
	gcc -c test_lmbench_compare.c -O3

clean:
	rm *.o main 

run:
	./main
