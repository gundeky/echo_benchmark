##### suffixes #####
.SUFFIXES : .cpp .o

##### user macro #####
UNAME_S:=$(shell uname -s)
ifeq ($(UNAME_S),Linux)
# BOOST_HOME=/home/gwlee/work/boost/local
BOOST_HOME=/home/gulee/work/boost/local
LDFLAGS=-lpthread ${BOOST_HOME}/lib/libboost_thread.a ${BOOST_HOME}/lib/libboost_system.a -lrt
endif
ifeq ($(UNAME_S),Darwin)
BOOST_HOME=/Users/gwlee/Work/boost/local
LDFLAGS=-lpthread ${BOOST_HOME}/lib/libboost_thread.a ${BOOST_HOME}/lib/libboost_system.a
endif

HOME=
TARGET=test
INCLUDES=-I${BOOST_HOME}/include
#LDFLAGS=-lpthread ${BOOST_HOME}/lib/libboost_thread.a ${BOOST_HOME}/lib/libboost_system.a -lrt
SRCS=test.cpp
OBJS=$(SRCS:.cpp=.o)

##### compiles #####
all : $(TARGET)
#chmod 700 $(TARGET)

$(TARGET) : $(OBJS)
	$(CXX) $(OBJS) $(LDFLAGS) -Wall -O2 -o $@
	#$(CXX) $(OBJS) $(LDFLAGS) -Wall -g -o $@

.cpp.o :
	$(CXX) $(INCLUDES) -Wall -O2 -c $<
	#$(CXX) $(INCLUDES) -Wall -g -c $<
#$(CXX) $(INCLUDES) -c -o $@ $<

clean :
	rm -rf $(OBJS) $(TARGET) core*

dep :
	gcc -MM $(INCLUDES) $(SRCS) > .depend

-include .depend
