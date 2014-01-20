CFLAGS = -Wall -DXP_UNIX=1 -fPIC -g `pkg-config --cflags glib-2.0 --libs json-glib-1.0`

apps2desktop : a2d-plugin.o a2d-main.o
	gcc $(CFLAGS) -shared a2d-plugin.o a2d-main.o -o apps2desktop.so

a2d-plugin.o : a2d-plugin.c a2d-plugin.h
	gcc $(CFLAGS) -c a2d-plugin.c

a2d-main.o : a2d-main.c
	gcc $(CFLAGS) -c a2d-main.c

clean :
	rm *.so *.o

