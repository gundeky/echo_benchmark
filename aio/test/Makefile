##### suffixes #####
.SUFFIXES : .cpp .o

##### user macro #####
TARGET=test
CPL_HOME=../cpl
AIO_HOME=../unix
INCLUDES=-I${CPL_HOME} -I${AIO_HOME}
LDFLAGS=-lpthread

#CXXFLAGS=-Wall -O3
CXXFLAGS=-Wall -g -DDEBUG
#CXXFLAGS=-Wall -g --coverage -fprofile-arcs -ftest-coverage -fno-inline -fno-inline-small-functions -fno-default-inline

USERID:=$(shell whoami)
UNAME_S:=$(shell uname -s)
ifeq ($(UNAME_S),Linux)
BOOST_HOME=/home/${USERID}/work/boost/local
INCLUDES+=-I${BOOST_HOME}/include
SRCS=${AIO_HOME}/proactor.cpp ${AIO_HOME}/proactor_epoll.cpp signal.cpp main.cpp
CXXFLAGS+=-std=c++11
endif
ifeq ($(UNAME_S),Darwin)
BOOST_HOME=/Users/${USERID}/Work/boost/local
INCLUDES+=-I${BOOST_HOME}/include
SRCS=${AIO_HOME}/proactor.cpp ${AIO_HOME}/proactor_kqueue.cpp signal.cpp main.cpp
CXX=clang++
CXXFLAGS+=-stdlib=libc++
CXXFLAGS+=-std=c++11
endif

OBJS=$(SRCS:.cpp=.o)

##### compiles #####
all : $(TARGET)

$(TARGET) : $(OBJS)
	$(CXX) $(OBJS) $(LDFLAGS) $(CXXFLAGS) -o $@

.cpp.o :
	$(CXX) $(INCLUDES) $(CXXFLAGS) -c -o $@ $<
#	$(CXX) $(INCLUDES) -c $<

clean :
	rm -rf $(OBJS) $(TARGET) core* *.gcov *.gcno *.gcda a.log ${AIO_HOME}/*.gcov ${AIO_HOME}/*.gcno ${AIO_HOME}/*.gcda

dep :
	gcc -MM $(INCLUDES) $(SRCS) > .depend

-include .depend
