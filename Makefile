CXX=g++
CXXFLAGS+=-Wall -Wextra -I./markov -std=gnu++0x
LDFLAGS+=-lPocoNet -lPocoFoundation
all: web

web: web.o markov.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^

markov.o: markov/markov.o
	cp $< $@

markov/markov.o: markov/markov.cpp markov/markov.h
	$(MAKE) -C markov markov.o
