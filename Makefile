common.o: common.c common.h
	gcc -c common.c

chat: SimpleChat.c common.o
	gcc common.o SimpleChat.c -o SimpleChat

switch: SimpleVswitch.c common.o
	gcc SimpleVswitch.c common.o -o OverlaySwitch

controller: controller.c common.o
	gcc controller.c common.o -o controller

clean:
	rm -f SimpleChat OverlaySwitch controller *.o
