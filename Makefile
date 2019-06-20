CXX		  := mpic++
CXX_FLAGS := -g -Wall -Wextra -std=c++17 -O2

ADIOS2_DIR := /opt/adios2
ADIOS2_INC = `${ADIOS2_DIR}/bin/adios2-config --cxx-flags`
ADIOS2_LIB = `${ADIOS2_DIR}/bin/adios2-config --cxx-libs`


all: fifo adios

fifo: main_fifo.cpp
	$(CXX) $(CXX_FLAGS) -I. $^ -o $@

adios: main_adios.cpp
	$(CXX) $(CXX_FLAGS) -I. $(ADIOS2_INC) $^ -o $@ ${ADIOS2_LIB}

clean:
	rm -f adios fifo