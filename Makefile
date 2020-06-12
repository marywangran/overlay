
chat: SimpleChat.c list.h
	gcc SimpleChat.c -o SimpleChat

switch: SimpleVswitch.c list.h
	gcc SimpleVswitch.c -o OverlaySwitch

controller: controller.c list.h
	gcc controller.c -o controller

clean:
	rm -f SimpleChat OverlaySwitch controller *.o
