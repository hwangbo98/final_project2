CC	=	g++
CPPFLAGS	= 	-std=c++11 #-Wall 
LIBS	=	-lboost_system -lcrypto -lssl -lpthread

all : chat_client chat_server

chat_client : chat_client.cpp 
	$(CC) $(CPPFLAGS) $^ -o $@ $(LIBS)

chat_server : chat_server.cpp
	$(CC) $(CPPFLAGS) $^ -o $@ $(LIBS)

clean:
	rm -f chat_client
	rm -f chat_server