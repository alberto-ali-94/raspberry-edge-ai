control_center: main.o io.o temperature.o gpio.o thermistor.o
	gcc -std=c11 -Wall -Wextra -Wpedantic -pthread main.o io.o temperature.o gpio.o thermistor.o -o control_center -lm
temperature.o: temperature.c temperature.h
	gcc -std=c11 -Wall -Wextra -Wpedantic -pthread -c temperature.c -o temperature.o
io.o: io.c io.h
	gcc -std=c11 -Wall -Wextra -Wpedantic -pthread -c io.c -o io.o
gpio.o: gpio.c gpio.h
	gcc -std=c11 -Wall -Wextra -Wpedantic -pthread -c gpio.c -o gpio.o
thermistor.o: thermistor.c thermistor.h
	gcc -std=c11 -Wall -Wextra -Wpedantic -pthread -c thermistor.c -o thermistor.o
main.o: main.c io.h temperature.h gpio.h thermistor.h
	gcc -std=c11 -Wall -Wextra -Wpedantic -pthread -c main.c -o main.o
