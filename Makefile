# Makefile for systems with GNU tools
# Added by hand
BSC_RUNTIME_INCLUDES=\
	-I/home/xpf/bsc_project/llvm-project/libcbs/src/string \
	-I/home/xpf/bsc_project/llvm-project/libcbs/src/hash \
	-I/home/xpf/bsc_project/llvm-project/libcbs/src/raw_vec \
	-I/home/xpf/bsc_project/llvm-project/libcbs/src/cell \
	-I/home/xpf/bsc_project/llvm-project/libcbs/src/string \
	-I/home/xpf/bsc_project/llvm-project/libcbs/src/raw_table \
	-I/home/xpf/bsc_project/llvm-project/libcbs/src/vec \
	-I/home/xpf/bsc_project/llvm-project/libcbs/src/map \
	-I/home/xpf/bsc_project/llvm-project/libcbs/src/rc \
	-I/home/xpf/bsc_project/llvm-project/libcbs/src/result \
	-I/home/xpf/bsc_project/llvm-project/libcbs/src/list \
	-I/home/xpf/bsc_project/llvm-project/libcbs/src/option \
	-I/home/xpf/bsc_project/llvm-project/libcbs/src/bishengc_safety \
	-I/home/xpf/bsc_project/llvm-project/libcbs/src/scheduler

BSC_RUNTIME_SRCS=\
	/home/xpf/bsc_project/llvm-project/libcbs/src/hash/sip.cbs \
	/home/xpf/bsc_project/llvm-project/libcbs/src/string/string.cbs \
	/home/xpf/bsc_project/llvm-project/libcbs/src/raw_table/raw_table.cbs \
	/home/xpf/bsc_project/llvm-project/libcbs/src/bishengc_safety/bishengc_safety.cbs \
	/home/xpf/bsc_project/llvm-project/libcbs/src/scheduler/scheduler.cbs

BSC_RUNTIME_LIBS=-L/home/xpf/bsc_project/llvm-project/build/lib/ -lstdcbs

LLVM_HOME = /home/xpf/bsc_project/llvm-project/build/bin
BSC_CLANG = $(LLVM_HOME)/clang

BSC_INCLUDE_FLAGS= $(BSC_RUNTIME_INCLUDES)
BSC_LINK_INPUTS=$(BSC_RUNTIME_LIBS) $(BSC_RUNTIME_SRCS)

# change CC by hand
CC 	= 	$(BSC_CLANG)
INSTALL	=	install
IFLAGS  = -idirafter dummyinc
#CFLAGS = -g
CFLAGS	=	-O2 -fPIE -fstack-protector --param=ssp-buffer-size=4 \
	-Wall -W -Wshadow -Werror -Wformat-security \
	-D_FORTIFY_SOURCE=2 \
	#-pedantic -Wconversion

LIBS	=	`./vsf_findlibs.sh`
LINK	=	-Wl,-s
LDFLAGS	=	-fPIE -pie -Wl,-z,relro -Wl,-z,now

OBJS	=	main.o utility.o prelogin.o ftpcmdio.o postlogin.o privsock.o \
	tunables.o ftpdataio.o secbuf.o ls.o \
	postprivparent.o logging.o str.o netstr.o sysstr.o strlist.o \
    banner.o filestr.o parseconf.o secutil.o \
    ascii.o oneprocess.o twoprocess.o privops.o standalone.o hash.o \
    tcpwrap.o ipaddrparse.o access.o features.o readwrite.o opts.o \
    ssl.o sslslave.o ptracesandbox.o ftppolicy.o sysutil.o sysdeputil.o \
    seccompsandbox.o

.c.o:
	$(CC) -c $*.c $(CFLAGS) $(IFLAGS)

%.o: %.cbs
	$(CC) $(CFLAGS) $(IFLAGS) $(BSC_INCLUDE_FLAGS) -c $< -o $@

vsftpd: $(OBJS) 
	$(CC) -o vsftpd $(OBJS) $(LINK) $(LDFLAGS) $(LIBS) $(BSC_INCLUDE_FLAGS)

install:
	if [ -x /usr/local/sbin ]; then \
		$(INSTALL) -m 755 vsftpd /usr/local/sbin/vsftpd; \
	else \
		$(INSTALL) -m 755 vsftpd /usr/sbin/vsftpd; fi
	if [ -x /usr/local/man ]; then \
		$(INSTALL) -m 644 vsftpd.8 /usr/local/man/man8/vsftpd.8; \
		$(INSTALL) -m 644 vsftpd.conf.5 /usr/local/man/man5/vsftpd.conf.5; \
	elif [ -x /usr/share/man ]; then \
		$(INSTALL) -m 644 vsftpd.8 /usr/share/man/man8/vsftpd.8; \
		$(INSTALL) -m 644 vsftpd.conf.5 /usr/share/man/man5/vsftpd.conf.5; \
	else \
		$(INSTALL) -m 644 vsftpd.8 /usr/man/man8/vsftpd.8; \
		$(INSTALL) -m 644 vsftpd.conf.5 /usr/man/man5/vsftpd.conf.5; fi
	if [ -x /etc/xinetd.d ]; then \
		$(INSTALL) -m 644 xinetd.d/vsftpd /etc/xinetd.d/vsftpd; fi

clean:
	rm -f *.o *.swp vsftpd

