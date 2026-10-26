control_center: main.o thermistor.o
	gcc -std=c11 -Wall -Wextra -Wpedantic main.o thermistor.o -o control_center -lm
thermistor.o: thermistor.c thermistor.h
	gcc -std=c11 -Wall -Wextra -Wpedantic -c thermistor.c -o thermistor.o
main.o: main.c io.h thermistor.h
	gcc -std=c11 -Wall -Wextra -Wpedantic -c main.c -o main.o
