.DEFAULT_GOAL := default

common.o: common.c common.h
	gcc -c common.c

chat: SimpleChat.c common.o
	gcc common.o SimpleChat.c -o SimpleChat

switch: SimpleVswitch.c common.o
	gcc SimpleVswitch.c common.o -o OverlaySwitch

SimpleSDWAN: SimpleSDWAN.c common.o
	gcc SimpleSDWAN.c common.o -o SimpleSDWAN

controller: controller.c common.o
	gcc controller.c common.o -o controller

all: controller chat switch SimpleSDWAN

default: all

clean:
	rm -f SimpleChat OverlaySwitch controller SimpleSDWAN *.o
