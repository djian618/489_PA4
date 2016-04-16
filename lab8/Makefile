CC = g++
CFLAGS = -g -Wall -Wno-deprecated
MKDEP=makedepend -Y
OS := $(shell uname)
ifeq ($(OS), Darwin)
  LIBS = -framework OpenGL -framework GLUT
else
  LIBS = -lGL -lGLU -lglut
endif

BINS = netimg wfqdb
HDRS = ltga.h socks.h netimg.h
SRCS = ltga.cpp netimglut.cpp socks.cpp netimg.cpp
HDRS_SLN = imgdb.h
SRCS_SLN = imgdb.cpp
OBJS = $(SRCS:.cpp=.o) $(SRCS_SLN:.cpp=.o)

all: $(BINS)

netimg: netimg.o netimglut.o socks.o $(HDRS)
	$(CC) $(CFLAGS) -o $@ $< netimglut.o socks.o $(LIBS)

wfqdb: imgdb.o ltga.o socks.o $(HDRS)
	$(CC) $(CFLAGS) -o $@ $< ltga.o socks.o
	
%.o: %.cpp
	$(CC) $(CFLAGS) $(INCLUDES) -c $<

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

.PHONY: clean
clean: 
	-rm -f -r $(OBJS) *.o *~ *core* netimg $(BINS)

depend: $(SRCS_SLN) $(HDRS_SLN) Makefile
	$(MKDEP) $(CFLAGS) $(SRCS_SLN) $(HDRS_SLN) >& /dev/null

# DO NOT DELETE

imgdb.o: imgdb.h
