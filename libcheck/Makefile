include ../Makefile.local

DEST            = ./libcheck

OBJECTS         = main.o ../ubc_check_verify.o
LIBS            = -lsha1detectcoll -lboost_filesystem -lboost_program_options -lboost_system -lboost_serialization -lboost_iostreams -lboost_random
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
	$(CXX) $(CXXFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CCFLAGS) -c $< -o $@
	
$(DEST): $(OBJECTS)
	$(CXX) $(LINKFLAGS) -o $(DEST) $(OBJECTS)  $(LIBS) $(LIBS)

