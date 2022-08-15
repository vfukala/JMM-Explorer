mkdir -p bin
bison src/parser.yy --defines=bin/parser.hpp -o bin/parser.cpp
