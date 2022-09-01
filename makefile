CXX=g++
CXXFLAGS=-g -Wall -Wextra -O3
CPPFLAGS=-Ibin -Isrc

SRC_FILES := $(wildcard src/*.cpp)
OBJ_FILES := $(patsubst src/%.cpp,bin/%.o,$(SRC_FILES))
OBJ_FILES += bin/parser.o bin/lexer.o
D_FILES := $(patsubst src/%.cpp,bin/%.d,$(SRC_FILES))
D_FILES += bin/parser.d bin/lexer.d

bin/jmmexplorer: $(OBJ_FILES)
	$(CXX) $(OBJ_FILES) -o bin/jmmexplorer $(LDFLAGS)

bin/test_jmmexplorer: $(filter-out bin/main.o,$(OBJ_FILES)) bin/test_main.o
	$(CXX) $(filter-out bin/main.o,$(OBJ_FILES)) bin/test_main.o -o bin/test_jmmexplorer $(LDFLAGS)

bin/parser.cpp bin/parser.hpp: src/parser.yy
	bison src/parser.yy --defines=bin/parser.hpp -o bin/parser.cpp

bin/lexer.cpp: src/lexer.l
	flex -t src/lexer.l > bin/lexer.cpp

bin/%.d: src/%.cpp
	@set -e; rm -f $@; \
		$(CXX) -MM $(CPPFLAGS) $< > $@.$$$$; \
		sed 's,\($*\)\.o[ :]*,bin/\1.o $@ : ,g' < $@.$$$$ > $@; \
		rm -f $@.$$$$

bin/%.o: src/%.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) src/$*.cpp -c -o $@

bin/test_main.o: src/main.cpp
	$(CXX) $(CPPFLAGS) -DTESTING $(CXXFLAGS) src/main.cpp -c -o bin/test_main.o

bin/parser.d: bin/parser.cpp
	@set -e; rm -f $@; \
		$(CXX) -MM $(CPPFLAGS) $< > $@.$$$$; \
		sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
		rm -f $@.$$$$

bin/lexer.d: bin/lexer.cpp
	@set -e; rm -f $@; \
		$(CXX) -MM $(CPPFLAGS) $< > $@.$$$$; \
		sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
		rm -f $@.$$$$

include $(D_FILES)
