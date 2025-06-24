CXX ?= g++
CXXFLAGS := -std=c++11 -I../components/uvr64_dlbus -Istubs -Istubs/esphome -Istubs/esphome/components -Istubs/esphome/core

all: test_parse_frame

# Build the test executable

test_parse_frame: test_parse_frame.cpp ../components/uvr64_dlbus/dlbus_sensor.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@

# Run the test
run: test_parse_frame
	./test_parse_frame

clean:
	rm -f test_parse_frame

.PHONY: all run clean
