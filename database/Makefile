CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -I.
MOCK_FLAGS = -DBTREE_USE_MOCK_PAGER

TARGET = database.exe
TEST_BTREE = test_btree.exe
TEST_INTEGRATION = test_integration.exe

SRCS = Main.cpp \
       Parse/parse.cpp \
       Parse/tokenize.cpp \
       Parse/sense.cpp \
       Execute/executor.cpp \
       Storage_engine/storage.cpp \
       Storage_engine/pager.cpp \
       Storage_engine/B+Tree/Btree.cpp \
       Storage_engine/B+Tree/StackPath.cpp \
       Help/Error/error.cpp \
       Help/Serialize/serialize.cpp \
       Help/Show/show.cpp

LIB_SRCS = Parse/parse.cpp \
           Parse/tokenize.cpp \
           Parse/sense.cpp \
           Execute/executor.cpp \
           Storage_engine/storage.cpp \
           Storage_engine/B+Tree/Btree.cpp \
           Storage_engine/B+Tree/StackPath.cpp \
           Storage_engine/B+Tree/mockpager.cpp \
           Help/Error/error.cpp \
           Help/Serialize/serialize.cpp \
           Help/Show/show.cpp

OBJS = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

test_btree: $(TEST_BTREE)
	./$(TEST_BTREE)

$(TEST_BTREE): Storage_engine/B+Tree/test_btree.cpp \
                Storage_engine/B+Tree/Btree.cpp \
                Storage_engine/B+Tree/StackPath.cpp \
                Storage_engine/B+Tree/mockpager.cpp
	$(CXX) $(CXXFLAGS) $(MOCK_FLAGS) $^ -o $@

test_integration: $(TEST_INTEGRATION)
	./$(TEST_INTEGRATION)

$(TEST_INTEGRATION): test_integration.cpp $(LIB_SRCS)
	$(CXX) $(CXXFLAGS) $(MOCK_FLAGS) $^ -o $@

check: test_btree test_integration

clean:
	rm -f $(OBJS) $(TARGET) $(TEST_BTREE) $(TEST_INTEGRATION)

.PHONY: all clean test_btree test_integration check
