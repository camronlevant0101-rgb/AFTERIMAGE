#!/usr/bin/env bash
# Builds and runs the DSP test-suite with plain g++ (no CMake / JUCE needed). Run from project root.
set -e
g++ -std=c++20 -O2 -g -Wall -Wextra -Wno-mismatched-new-delete -IDSP Tests/test_engine.cpp DSP/GranularEngine.cpp -o Build/afterimage_tests -pthread
./Build/afterimage_tests
# Optional sanitizer passes:
#   g++ -std=c++20 -O1 -g -fsanitize=address,undefined ...   (memory errors / UB)
#   g++ -std=c++20 -O1 -g -fsanitize=thread ...              (data races)
