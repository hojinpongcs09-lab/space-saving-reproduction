CXX ?= c++
CXXFLAGS ?= -O2 -std=c++17 -Wall -Wextra -Wpedantic
PYTHON ?= python3
CORE = ../../SpaceSaving_CountSketch_Integrated

.PHONY: all test reproduce
all: build/alert_validation
build/alert_validation: alert_validation.cpp $(CORE)/src/space_saving.cpp $(CORE)/include/space_saving.hpp
	mkdir -p build
	$(CXX) $(CXXFLAGS) -I$(CORE)/include alert_validation.cpp $(CORE)/src/space_saving.cpp -o $@
test: all
	$(PYTHON) test_alerts.py
reproduce: test
	$(PYTHON) run_examples.py
