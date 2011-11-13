CFLAGS=-g

xmlfs: srv.o xml.o main.o
	cc -g -o xmlfs srv.o xml.o main.o -lixp `pkg-config --libs libxml-2.0`

xml.o: xml.cpp
	g++ -g -c xml.cpp `pkg-config --cflags libxml-2.0`

clean:
	rm -f xmlfs *.o
