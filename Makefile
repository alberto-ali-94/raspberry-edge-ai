control_center: main.o io.o temperatura.o gpio.o
	gcc -std=c11 -Wall -Wextra -Wpedantic main.o io.o temperatura.o gpio.o -o control_center
temperatura.o: temperatura.c temperatura.h
	gcc -std=c11 -Wall -Wextra -Wpedantic -c temperatura.c -o temperatura.o
io.o: io.c io.h
	gcc -std=c11 -Wall -Wextra -Wpedantic -c io.c -o io.o
gpio.o: gpio.c gpio.h
	gcc -std=c11 -Wall -Wextra -Wpedantic -c gpio.c -o gpio.o
main.o: main.c io.h temperatura.h gpio.h
	gcc -std=c11 -Wall -Wextra -Wpedantic -c main.c -o main.o
