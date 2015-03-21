# C compiler:
CC      = gcc#
# C compiler options:
CFLAGS  = -O2 -Wall -ansi#
# Libraries:
LIBS    =#
# Install root-directory:
INDIR	=/usr/include/#
LIBDIR	=/usr/local/lib/#
BINDIR	=/usr/local/bin/#
# cgi-bin
WWW_CGIBIN = /home/httpd/html/cgi-bin/


# explicit rules
all: mht2html libcgimht.a

mem.o: mem.c mem.h
	$(CC) -c $(CFLAGS) mem.c
	
hash.o: hash.c hash.h mem.o
	$(CC) -c $(CFLAGS) hash.c

cgi.o: cgi.c cgi.h
	$(CC) -c $(CFLAGS) cgi.c
	
hash_test: hash_test.c hash.o
	$(CC) $(CFLAGS) hash_test.c hash.o -o hash_test
	chmod 755 $(BINPATH)hash_test

str_util.o: str_util.c str_util.h
	$(CC) -c $(CFLAGS) str_util.c
	
mht.o: mht.c mht.h str_util.o hash.o
	$(CC) -c $(CFLAGS) mht.c
	
mht2html: mht2html.c mem.o mht.o hash.o str_util.o
	$(CC) $(CFLAGS) mht2html.c mem.o mht.o hash.o str_util.o -o mht2html

contact: contact.c
	$(CC) $(CFLAGS) contact.c /usr/local/lib/libcgimht.a -o $(WWW_CGIBIN)contact.cgi	
	
libcgimht.a: mem.o hash.o cgi.o str_util.o mht.o
	ar -r libcgimht.a hash.o
	ar -r libcgimht.a cgi.o
	ar -r libcgimht.a str_util.o
	ar -r libcgimht.a mht.o
	ar -r libcgimht.a mem.o
	ranlib libcgimht.a
	touch libcgimht.a

install: clean mht2html libcgimht.a
	rm $(LIBDIR)libcgimht.a
	rm $(INDIR)mht.h
	rm $(INDIR)cgi.h
	rm $(BINDIR)mht2html
	cp libcgimht.a $(LIBDIR)
	cp mht.h $(INDIR)
	cp cgi.h $(INDIR)
	cp mht2html $(BINDIR)
	
clean:
	rm *.o