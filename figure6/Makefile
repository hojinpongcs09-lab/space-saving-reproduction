CXX ?= g++
CXXFLAGS ?= -O3 -std=c++17 -Wall -Wextra -Wpedantic -Iinclude
PYTHON ?= python3

.PHONY: all test smoke reproduce calibrate plot compare compare-smoke compare-plot clean
all: build/figure6_experiment build/figure6_comparison build/test_space_saving build/test_count_sketch

build:
	mkdir -p build

build/figure6_experiment: src/figure6_experiment.cpp src/space_saving.cpp include/space_saving.hpp | build
	$(CXX) $(CXXFLAGS) src/figure6_experiment.cpp src/space_saving.cpp -o $@

build/figure6_comparison: src/figure6_comparison.cpp src/space_saving.cpp src/count_sketch.cpp include/space_saving.hpp include/count_sketch.hpp | build
	$(CXX) $(CXXFLAGS) src/figure6_comparison.cpp src/space_saving.cpp src/count_sketch.cpp -o $@

build/test_space_saving: tests/test_space_saving.cpp src/space_saving.cpp include/space_saving.hpp | build
	$(CXX) $(CXXFLAGS) tests/test_space_saving.cpp src/space_saving.cpp -o $@

build/test_count_sketch: tests/test_count_sketch.cpp src/count_sketch.cpp include/count_sketch.hpp | build
	$(CXX) $(CXXFLAGS) tests/test_count_sketch.cpp src/count_sketch.cpp -o $@

test: build/test_space_saving build/test_count_sketch
	./build/test_space_saving
	./build/test_count_sketch

smoke: all
	mkdir -p results
	./build/figure6_experiment --events 1000000 --alphabet 1000 --counters 300 --output results/figure6_smoke.csv --validate
	$(PYTHON) scripts/plot_figure6.py --csv results/figure6_smoke.csv --outdir results/smoke

reproduce: build/figure6_experiment
	mkdir -p results
	./build/figure6_experiment --events 10000000 --alphabet 10000 --topk 100 --counters 1000 --output results/figure6_space_saving.csv
	$(PYTHON) scripts/plot_figure6.py

calibrate: build/figure6_experiment
	mkdir -p results
	./build/figure6_experiment --events 10000000 --alphabet 10000 --topk 100 --mode calibrate --budgets 100,200,400,800,1600,3200,6400,10000 --output results/figure6_calibrated.csv
	$(PYTHON) scripts/plot_figure6.py --csv results/figure6_calibrated.csv --outdir results/calibrated

plot:
	$(PYTHON) scripts/plot_figure6.py

# Same Zipf event stream per alpha for both algorithms; independent hash seed for CS.
compare: build/figure6_comparison
	mkdir -p results
	./build/figure6_comparison --events 10000000 --alphabet 10000 --topk 100 --counters 1000 --cs-width 8192 --cs-depth 7 --output results/figure6_comparison.csv
	$(PYTHON) scripts/plot_figure6_comparison.py --csv results/figure6_comparison.csv --outdir results/comparison

compare-smoke: build/figure6_comparison
	mkdir -p results
	./build/figure6_comparison --events 100000 --alphabet 1000 --topk 30 --counters 300 --cs-width 2048 --cs-depth 7 --validate --output results/figure6_comparison_smoke.csv
	$(PYTHON) scripts/plot_figure6_comparison.py --csv results/figure6_comparison_smoke.csv --outdir results/comparison_smoke

compare-plot:
	$(PYTHON) scripts/plot_figure6_comparison.py --csv results/figure6_comparison.csv --outdir results/comparison

clean:
	rm -rf build

# Secondary experiment: roughly matched data-structure memory on this 64-bit ABI
# (SS ~128KB vs CS ~121KB). Not the same configuration as the paper.
.PHONY: compare-equal-memory
compare-equal-memory: build/figure6_comparison
	mkdir -p results
	./build/figure6_comparison --events 10000000 --alphabet 10000 --topk 100 --counters 1000 --cs-width 2048 --cs-depth 7 --output results/figure6_equal_memory.csv
	$(PYTHON) scripts/plot_figure6_comparison.py --csv results/figure6_equal_memory.csv --outdir results/equal_memory
