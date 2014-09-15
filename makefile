UNAME = $(shell uname)
ifeq ($(UNAME), SunOS) # Sun OS
MY_LIBS = -lresolv -lsocket -lnsl
endif
ifeq ($(UNAME), Linux) # Linux
MY_LIBS = -lresolv -lnsl -lpthread
endif
ifeq ($(UNAME), Darwin) # Mac OS
MY_LIBS =
endif

CC := g++

all: Main

Main: sujaypar_server.cpp RoutingServer.cpp RoutingServer.h CustomStructs.h
	${CC} -m32 sujaypar_server.cpp RoutingServer.cpp -o server

##==========================================================================
clean:
	@- $(RM) server
	@- echo “Data Cleansing Done.Ready to Compile”


