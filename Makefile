#CC := g++
CC := clang++

compile:
	$(CC) src/suxml.cpp -o bin/suxml -lncurses -Wall -pedantic -Wno-long-long -O0 -ggdb --std=c++11
run:
	./bin/suxml
clean:
	rm /bin/suxml

all: compile doc
