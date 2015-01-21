CXX=clang++
CXXFLAGS=-c -std=c++11 -g -O2 -Wall -Wno-unused-function
LDFLAGS=-stdlib=libc++ -lpthread
SOURCES=$(wildcard src/*.cpp)
OBJECTS=$(addprefix obj/,$(notdir $(SOURCES:.cpp=.o)))
EXECUTABLE=bin/chess
DEPS=$(wildcard obj/*.d)

chess: $(OBJECTS)
	$(CXX) $(LDFLAGS) $(OBJECTS) -o $(EXECUTABLE)
	dsymutil $(EXECUTABLE)
	cp $(EXECUTABLE) ./

obj/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) $< -o $@
	$(CXX) -MM -MP -MT $@ -MT obj/$*.d $(CXXFLAGS) $< > obj/$*.d

-include $(DEPS)

test: chess
	bin/chess -test

git:
	git commit -a

clean:
	rm obj/*.o
	rm obj/*.d
	rm bin/*