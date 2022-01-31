CC=/usr/bin/gcc
CC_OPT=
LIBS = -lpth # -lpcre -lmxml -lgearman -lm
CDEBUG = -g -O0
CFLAGS = $(CDEBUG) $(CC_OPT) -I. $(DEFS) # -std=c99 -I. 
LDFLAGS = -g $(LIBS)

all: dummymail

dummymail: error.o smtpd.o dummymail.o socket.o test_common.o deliver_message.o snprintf.o rbl.o transact_udp.o lazy_getaddr.o

install:
	install dummymail /usr/local/sbin/dummymail

clean: 
	rm *.o # spamcop.o geturl.o socket.o sample.o smtp.o main.o geturl.o deliver_message.o dummymail.o error.o test_common.o socks4.o smtpd.o wp.o snprintf.o

socks4: test_common.o socket.o socks4.o geturl.o
	gcc -o socks4 socks4.o test_common.o socket.o geturl.o -lpth

spamcop_tar: spamcop
	tar zcvf spamcop.tgz Makefile spamcop.c socket.c socket.h geturl.c geturl.h pth_compat.h

sample: sample.o test_common.o geturl.o socket.o
	gcc -o stoxy test_common.o sample.o geturl.o socket.o -g -lpth

wp: wp.o geturl.o socket.o wp_process.o -lpth -lpcre

spamcop: spamcop.o geturl.o socket.o
	gcc -o spamcop spamcop.o geturl.o socket.o -g -lpth

smtp: error.o smtp.o main.o socket.o test_common.o
	gcc -o smtp socket.o test_common.o smtp.o main.o error.o snprintf.o -g -lpth


# gcc -o dummymail socket.o test_common.o smtpd.o dummymail.o error.o deliver_message.o snprintf.o -g -lpth

udns: 	error.o dns.o udns.o socket.o test_common.o lookup.o
	gcc -o udns socket.o test_common.o dns.o udns.o error.o lookup.o -lpth


sample.o: sample.c
	gcc -c -o sample.o sample.c -g 

test_common.o: test_common.c test_common.h
	gcc -c -o test_common.o test_common.c -g

error.o: error.c error.h
	gcc -c -o error.o error.c -g

smtp.o: smtp.c smtp.h
	gcc -c -o smtp.o smtp.c -g

wp.o: wp.c 
	gcc -c -o wp.o wp.c -g

wp_process.o: wp_process.c wp_process.h
	gcc -std=gnu9x -c -o wp_process.o wp_process.c -g 

smtpd.o: smtpd.c smtp.h
	gcc -c -o smtpd.o smtpd.c -g

lookup.o: lookup.c
	gcc -c -o lookup.o lookup.c -g

dns.o: dns.c
	gcc -c -o dns.c -g


socket.o: socket.c socket.h
	gcc -c -o socket.o socket.c -g 

geturl.o: geturl.c geturl.h
	gcc -c -o geturl.o geturl.c -g

main.o: main.c
	gcc -c -o main.o main.c -g

spamcop.o: spamcop.c
	gcc -c -o spamcop.o spamcop.c -g

dummymail.o: dummymail.c
	gcc -c -o dummymail.o dummymail.c -g

deliver_message.o: deliver_message.c deliver_message.h
	gcc -c -o deliver_message.o deliver_message.c -g

socks4.o: socks4.c
	gcc -c -o socks4.o socks4.c -g 

snprintf.o: snprintf.c snprintf.h
	gcc -c -o snprintf.o snprintf.c -g
