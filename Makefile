aavga.so: aavga.c Makefile
	gcc -Wl,-soname,libvga.so.1 -o aavga.so -shared -nostdlib -fPIC aavga.c -laa -lX11 -lm -lgpm -lc -O2 -fomit-frame-pointer -funroll-all-loops -L /usr/X11R6/lib -lncurses
	#i486-linuxlibc1-gcc -Wl,-soname,libvga.so.1 -o aavga.so -shared -nostdlib -fPIC aavga.c -laa -lX11 -lm -lgpm -lc -O2 -fomit-frame-pointer -funroll-all-loops #-L /usr/X11R6/lib
