DEPS = file-system.h
	
%.o: %.c $(DEPS)
	gcc -c -std=c11 -Wall -ggdb3 -o $@ $<

file-system: main-test.o file-system.o
	gcc -o file-system main-test.o file-system.o

clean:
	rm -rf *.o file-system

.PHONY: clean