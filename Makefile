control_center: main.o io.o temperature.o gpio.o
	gcc -std=c11 -Wall -Wextra -Wpedantic main.o io.o temperature.o gpio.o -o control_center
temperature.o: temperature.c temperature.h
	gcc -std=c11 -Wall -Wextra -Wpedantic -c temperature.c -o temperature.o
io.o: io.c io.h
	gcc -std=c11 -Wall -Wextra -Wpedantic -c io.c -o io.o
gpio.o: gpio.c gpio.h
	gcc -std=c11 -Wall -Wextra -Wpedantic -c gpio.c -o gpio.o
thermistor.o: thermistor.c thermistor.h
	gcc -std=c11 -Wall -Wextra -Wpedantic -c thermistor.c -o thermistor.o
main.o: main.c io.h temperature.h gpio.h
	gcc -std=c11 -Wall -Wextra -Wpedantic -c main.c -o main.o
