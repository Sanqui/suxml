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
	cd src && doxygen && mv html ../doc && cd -
install:
	cp suxml $(INSTALL_PATH)/bin

all: compile doc
