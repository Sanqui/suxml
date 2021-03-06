NAME := suxml

CC := g++
#CC := clang++

INSTALL_PATH := /usr/local

compile:
	$(CC) src/suxml.cpp -o ${NAME} -lncurses -Wall -pedantic -Wno-long-long -O0 -ggdb --std=c++11
run:
	./${NAME}
clean:
	rm -rf ${NAME} doc/
doc:
	doxygen && mv html doc
install:
	cp $(NAME) $(INSTALL_PATH)/bin
	cp suxml.1 $(INSTALL_PATH)/share/man/man1

all: compile doc
