CXX ?= g++
CXXFLAGS ?= -O3 -std=c++17 -Wall -Wextra -Wpedantic -Iinclude
PYTHON ?= python3

.PHONY: all test smoke reproduce plot compare-fixed compare-ss-budgets \
        windowed-smoke windowed-experiment windowed-plot clean

all: build/figure6_comparison build/test_space_saving build/test_count_sketch \
     build/windowed_experiment build/test_windowed_space_saving

build:
	mkdir -p build

build/figure6_comparison: src/figure6_comparison.cpp src/space_saving.cpp src/count_sketch.cpp include/space_saving.hpp include/count_sketch.hpp | build
	$(CXX) $(CXXFLAGS) src/figure6_comparison.cpp src/space_saving.cpp src/count_sketch.cpp -o $@

build/test_space_saving: tests/test_space_saving.cpp src/space_saving.cpp include/space_saving.hpp | build
	$(CXX) $(CXXFLAGS) tests/test_space_saving.cpp src/space_saving.cpp -o $@

build/test_count_sketch: tests/test_count_sketch.cpp src/count_sketch.cpp include/count_sketch.hpp | build
	$(CXX) $(CXXFLAGS) tests/test_count_sketch.cpp src/count_sketch.cpp -o $@

build/windowed_experiment: src/windowed_experiment.cpp src/windowed_space_saving.cpp src/space_saving.cpp include/windowed_space_saving.hpp include/periodic_reset_space_saving.hpp include/exact_sliding_window.hpp include/space_saving.hpp | build
	$(CXX) $(CXXFLAGS) src/windowed_experiment.cpp src/windowed_space_saving.cpp src/space_saving.cpp -o $@

build/test_windowed_space_saving: tests/test_windowed_space_saving.cpp src/windowed_space_saving.cpp src/space_saving.cpp include/windowed_space_saving.hpp include/exact_sliding_window.hpp include/space_saving.hpp | build
	$(CXX) $(CXXFLAGS) tests/test_windowed_space_saving.cpp src/windowed_space_saving.cpp src/space_saving.cpp -o $@

test: build/test_space_saving build/test_count_sketch build/test_windowed_space_saving
	./build/test_space_saving
	./build/test_count_sketch
	./build/test_windowed_space_saving

smoke: all
	mkdir -p results/smoke
	./build/figure6_comparison --events 100000 --alphabet 500 --topk 20 --cs-width 512 --cs-depth 5 --validate --output results/smoke/measurements.csv
	$(PYTHON) scripts/plot_figure6_comparison.py --csv results/smoke/measurements.csv --outdir results/smoke

reproduce: build/figure6_comparison
	mkdir -p results
	./build/figure6_comparison --events 10000000 --alphabet 10000 --topk 100 --ss-budget theory --cs-width 8192 --cs-depth 7 --repeats 3 --output results/figure6_comparison.csv
	$(PYTHON) scripts/plot_figure6_comparison.py --csv results/figure6_comparison.csv --outdir results/comparison

plot:
	$(PYTHON) scripts/plot_figure6_comparison.py --csv results/figure6_comparison.csv --outdir results/comparison

compare-fixed: build/figure6_comparison
	mkdir -p results/fixed
	./build/figure6_comparison --events 1000000 --alphabet 10000 --topk 100 --ss-budget fixed --counters 1000 --cs-width 8192 --cs-depth 7 --output results/fixed/measurements.csv
	$(PYTHON) scripts/plot_figure6_comparison.py --csv results/fixed/measurements.csv --outdir results/fixed

compare-ss-budgets: build/figure6_comparison
	mkdir -p results
	./build/figure6_comparison --ss-budget-comparison --ss-budget theory --counters 1000 --events 10000000 --alphabet 10000 --topk 100 --repeats 3 --stream-order iid --validate --output results/space_saving_budget_comparison.csv
	$(PYTHON) scripts/plot_ss_budget_comparison.py --csv results/space_saving_budget_comparison.csv --outdir results/space_saving_budget_comparison --fixed-counters 1000

windowed-smoke: build/windowed_experiment
	mkdir -p results/windowed
	./build/windowed_experiment --events 200000 --alphabet 100 --window 20000 --counters 80 --topk 10 --query-interval 10000 --repeats 1 --alphas 1,1.5 --validate --output results/windowed/smoke.csv

windowed-experiment: build/windowed_experiment
	mkdir -p results/windowed
	./build/windowed_experiment --validate --output results/windowed/windowed_comparison.csv

windowed-plot:
	$(PYTHON) scripts/plot_windowed_results.py --csv results/windowed/windowed_comparison.csv --outdir results/windowed/plots

clean:
	rm -rf build

# Expanded reproducibility / sensitivity experiments. Old results are retained.
.PHONY: sensitivity-ss sensitivity-window sensitivity-plot sensitivity test-ties
build/test_topk_metrics: tests/test_topk_metrics.cpp include/topk_metrics.hpp | build
	$(CXX) $(CXXFLAGS) $< -o $@

test-ties: build/test_topk_metrics
	./build/test_topk_metrics

test: test-ties test-periodic

build/test_periodic_reset_space_saving: tests/test_periodic_reset_space_saving.cpp src/space_saving.cpp include/periodic_reset_space_saving.hpp include/space_saving.hpp | build
	$(CXX) $(CXXFLAGS) tests/test_periodic_reset_space_saving.cpp src/space_saving.cpp -o $@

.PHONY: test-periodic
test-periodic: build/test_periodic_reset_space_saving
	./build/test_periodic_reset_space_saving


build/figure6_comparison build/windowed_experiment: include/topk_metrics.hpp

sensitivity-ss: build/figure6_comparison
	mkdir -p results/sensitivity
	./build/figure6_comparison --ss-budget-sweep --events 10000000 --alphabet 10000 --topk 100 --repeats 3 --stream-order iid --validate --output results/sensitivity/ss_sweep.csv

sensitivity-window: build/windowed_experiment
	mkdir -p results/sensitivity
	set -e; for m in 400 800 1600 3200 6400 10000; do \
	  ./build/windowed_experiment --counters $$m --scenarios all --validate --output results/sensitivity/window_M$$m.csv; \
	done

sensitivity-plot:
	$(PYTHON) scripts/plot_sensitivity.py --dir results/sensitivity

sensitivity:
	$(MAKE) sensitivity-ss
	$(MAKE) sensitivity-window
	$(MAKE) sensitivity-plot

# Run the periodic baseline and other windowed methods under identical query
# times, then validate that the periodic summary covers the full W only at
# multiples of W. Output goes to a new CSV, preserving historical results.
.PHONY: periodic-comparison
periodic-comparison: build/windowed_experiment
	mkdir -p results/periodic
	./build/windowed_experiment --validate --output results/periodic/windowed_with_periodic.csv
	$(PYTHON) scripts/check_periodic_csv.py results/periodic/windowed_with_periodic.csv
	$(PYTHON) scripts/plot_windowed_results.py --csv results/periodic/windowed_with_periodic.csv --outdir results/periodic/plots
