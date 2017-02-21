include ../Makefile.local

DEST            = ./ubc_check_test

OBJECTS         = main.o test_simd.o _ubc_check.o _ubc_check_simd_avx256.o _ubc_check_simd_mmx64.o _ubc_check_simd_sse128.o _ubc_check_simd_neon128.o test_basic.o test_simd_mmx64.o test_simd_sse128.o test_simd_avx256.o test_simd_neon128.o
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
	$(CXX) $(LINKFLAGS) -o $(DEST) $(OBJECTS) $(LIBS) $(LIBS)

