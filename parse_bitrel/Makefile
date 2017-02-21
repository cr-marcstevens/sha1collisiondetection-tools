include ../Makefile.local

DEST            = ./parse_bitrel

OBJECTS         = parse_bitrel.o 
LIBS            = -lboost_filesystem -lboost_program_options -lboost_system -lboost_serialization -lboost_iostreams -lboost_random
MKPROPER	= *~

all: $(DEST)

run: $(DEST)
	./$(DEST) 2>&1 | tee makerun.log

clean:
	rm -f $(DEST) $(OBJECTS)

proper: clean
	rm -f $(MKPROPER)
	rm -f -r ./.tmp

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -I. -I.. -c $<
	
$(DEST): $(OBJECTS)
	$(CXX) $(LINKFLAGS) -o $(DEST) $(OBJECTS)  $(LIBS) $(LIBS)

