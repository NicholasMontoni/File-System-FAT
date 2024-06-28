DEPS = file-system.h

%.o: %.c $(DEPS)
	gcc -c -o $@ $<

file-system: main-test.o file-system.o
	gcc -o file-system main-test.o file-system.o