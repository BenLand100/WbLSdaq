CFLAGS = -Wall -Werror -pedantic -g -O3 -std=c++11 -DLINUX -Isrc
LFLAGS = -lhdf5_cpp -lhdf5 -lCAENVME -pthread -s

LSRC = $(wildcard src/*.cc)
LHED = $(LSRC:.cc=.hh)
LOBJ = $(addprefix build/,$(notdir $(LSRC:.cc=.o)))
LDEP = $(LOBJ:.o=.d)

BSRC = $(wildcard src/*.cxx)
BOBJ = $(addprefix build/,$(notdir $(BSRC:.cxx=.o)))
BDEP = $(BOBJ:.o=.d)

BINS = $(notdir $(basename $(BSRC))) 

all: $(BINS)

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

-include $(LDEP) $(BDEP)

$(BOBJ): build/%.o: src/%.cxx build/%.d
	$(CXX) $(CFLAGS) -c $< -o $@

$(LOBJ): build/%.o: src/%.cc build/%.d
	$(CXX) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(BDEP) $(BOBJ) $(LDEP) $(LOBJ) $(BINS)

