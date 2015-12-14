CFLAGS = -march=native -mtune=native -Wall -Werror -pedantic -g -O3 -std=c++11 -DLINUX -Isrc
LFLAGS = -lhdf5_cpp -lhdf5 -lCAENVME -lCAENDigitizer -pthread -s

# component object for each src/*.cc with header src/*.hh
LSRC = $(wildcard src/*.cc)
LHED = $(LSRC:.cc=.hh)
LOBJ = $(addprefix build/,$(notdir $(LSRC:.cc=.o)))
LDEP = $(LOBJ:.o=.d)

# output binary for each src/*.cxx
BSRC = $(wildcard src/*.cxx)
BOBJ = $(addprefix build/,$(notdir $(BSRC:.cxx=.o)))
BDEP = $(BOBJ:.o=.d)

BINS = $(notdir $(basename $(BSRC))) 

all: $(BINS)
	@echo Finished building WbLSdaq

# binaries depend on all component objects
$(BINS): %: build/%.o $(LOBJ)
	$(CXX) $< $(LOBJ) $(LFLAGS) -o $@

$(BDEP): build/%.d: src/%.cxx
	@mkdir -p build
	@set -e; rm -f $@
	$(CXX) -M $(CFLAGS) -MT $(<:.cxx=.o) $< > $@
	@sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' -i $@

$(LDEP): build/%.d: src/%.cc src/%.hh
	@mkdir -p build
	@set -e; rm -f $@
	$(CXX) -M $(CFLAGS) -MT $(<:.cc=.o) $< > $@
	@sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' -i $@

# these won't exist the first build
-include $(LDEP) $(BDEP)

$(BOBJ): build/%.o: build/%.d
	$(CXX) $(CFLAGS) -c $(addprefix src/,$(notdir $(<:.d=.cxx))) -o $@

$(LOBJ): build/%.o: build/%.d
	$(CXX) $(CFLAGS) -c $(addprefix src/,$(notdir $(<:.d=.cc))) -o $@

clean:
	rm -f $(BDEP) $(BOBJ) $(LDEP) $(LOBJ) $(BINS)

