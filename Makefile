# CC=/usr/bin/$(CC)
CC_OPT=
LIBS = -lpth # -lpcre -lmxml -lgearman -lm
# CDEBUG = -g -O0
CFLAGS = $(CDEBUG) $(CC_OPT) -I. $(DEFS) # -std=c99 -I. 
# LDFLAGS = -g $(LIBS)

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

all: dummymail

dummymail: error.o smtpd.o dummymail.o socket.o test_common.o deliver_message.o snprintf.o rbl.o transact_udp.o lazy_getaddr.o
	$(CC) -o dummymail error.o smtpd.o dummymail.o socket.o test_common.o deliver_message.o snprintf.o rbl.o transact_udp.o lazy_getaddr.o $(LIBS)

sample: sample.o test_common.o geturl.o socket.o
	$(CC) -o stoxy test_common.o sample.o geturl.o socket.o -g $(LIBS)


install:
	install dummymail /usr/local/sbin/dummymail

clean: 
	rm -f *.o # spamcop.o geturl.o socket.o sample.o smtp.o main.o geturl.o deliver_message.o dummymail.o error.o test_common.o socks4.o smtpd.o wp.o snprintf.o

socks4: test_common.o socket.o socks4.o geturl.o
	$(CC) -o socks4 socks4.o test_common.o socket.o geturl.o $(LIBS)

spamcop_tar: spamcop
	tar zcvf spamcop.tgz Makefile spamcop.c socket.c socket.h geturl.c geturl.h pth_compat.h

wp: wp.o geturl.o socket.o wp_process.o $(LIBS) -lpcre

spamcop: spamcop.o geturl.o socket.o
	$(CC) -o spamcop spamcop.o geturl.o socket.o -g $(LIBS)

# smtp: error.o smtp.o main.o socket.o test_common.o
	# $(CC) -o smtp socket.o test_common.o smtp.o main.o error.o snprintf.o -g $(LIBS)


# $(CC) -o dummymail socket.o test_common.o smtpd.o dummymail.o error.o deliver_message.o snprintf.o -g $(LIBS)

udns: 	error.o dns.o udns.o socket.o test_common.o lookup.o
	$(CC) -o udns socket.o test_common.o dns.o udns.o error.o lookup.o $(LIBS)


# sample.o: sample.c
	# $(CC) -c -o sample.o sample.c -g 
# 
# test_common.o: test_common.c test_common.h
	# $(CC) -c -o test_common.o test_common.c -g
# 
# error.o: error.c error.h
	# $(CC) -c -o error.o error.c -g
# 
# smtp.o: smtp.c smtp.h
	# $(CC) -c -o smtp.o smtp.c -g
# 
# wp.o: wp.c 
	# $(CC) -c -o wp.o wp.c -g
# 
# wp_process.o: wp_process.c wp_process.h
	# $(CC) -std=gnu9x -c -o wp_process.o wp_process.c -g 
# 
# smtpd.o: smtpd.c smtp.h
	# $(CC) -c -o smtpd.o smtpd.c -g
# 
# lookup.o: lookup.c
	# $(CC) -c -o lookup.o lookup.c -g
# 
# dns.o: dns.c
	# $(CC) -c -o dns.c -g
# 
# 
# socket.o: socket.c socket.h
	# $(CC) -c -o socket.o socket.c -g 
# 
# geturl.o: geturl.c geturl.h
	# $(CC) -c -o geturl.o geturl.c -g
# 
# main.o: main.c
	# $(CC) -c -o main.o main.c -g
# 
# spamcop.o: spamcop.c
	# $(CC) -c -o spamcop.o spamcop.c -g
# 
# dummymail.o: dummymail.c
	# $(CC) -c -o dummymail.o dummymail.c -g
# 
# deliver_message.o: deliver_message.c deliver_message.h
	# $(CC) -c -o deliver_message.o deliver_message.c -g
# 
# socks4.o: socks4.c
	# $(CC) -c -o socks4.o socks4.c -g 
# 
# snprintf.o: snprintf.c snprintf.h
	# $(CC) -c -o snprintf.o snprintf.c -g
