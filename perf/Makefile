include ../Makefile.local

DEST            = perftest

OBJECTS         = main.o random_hashing.o
LIBS            = -lsha1detectcoll -lboost_program_options -lboost_system -lboost_timer -lboost_chrono
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

