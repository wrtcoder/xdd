# XDD Makefile

#
# Configuration
#
SHELL 	=	/bin/sh
OS 	= 	$(shell uname)

#
# Version information
#
DATESTAMP =	$(shell date +%m%d%y )
BUILD 	=	$(shell date +%H%M )
PROJECT =	xdd
VERSION =	7.0.0.rc12
XDD_VERSION = $(OS).$(VERSION).$(DATESTAMP).Build.$(BUILD)
XDDVERSION = \"$(XDD_VERSION)\"

#
# Source locations
#
HDR_DIR = src
SRC_DIR = src

#
# Source files
#
XDD_SOURCE = $(SRC_DIR)/access_pattern.c \
	$(SRC_DIR)/barrier.c \
	$(SRC_DIR)/datapatterns.c \
	$(SRC_DIR)/debug.c \
	$(SRC_DIR)/end_to_end.c \
	$(SRC_DIR)/global_clock.c \
	$(SRC_DIR)/global_data.c \
	$(SRC_DIR)/global_time.c \
	$(SRC_DIR)/heartbeat.c \
	$(SRC_DIR)/info_display.c \
	$(SRC_DIR)/initialization.c \
	$(SRC_DIR)/io_buffers.c \
	$(SRC_DIR)/io_loop.c \
	$(SRC_DIR)/io_loop_after_io_operation.c \
	$(SRC_DIR)/io_loop_after_loop.c  \
	$(SRC_DIR)/io_loop_before_io_operation.c \
	$(SRC_DIR)/io_loop_before_loop.c \
	$(SRC_DIR)/io_loop_perform_io_operation.c \
	$(SRC_DIR)/io_thread.c \
	$(SRC_DIR)/io_thread_cleanup.c \
	$(SRC_DIR)/io_thread_init.c \
	$(SRC_DIR)/memory.c \
	$(SRC_DIR)/parse.c \
	$(SRC_DIR)/parse_func.c \
	$(SRC_DIR)/parse_table.c \
	$(SRC_DIR)/pclk.c \
	$(SRC_DIR)/processor.c \
	$(SRC_DIR)/ptds.c \
	$(SRC_DIR)/read_after_write.c \
	$(SRC_DIR)/restart.c \
	$(SRC_DIR)/results_display.c \
	$(SRC_DIR)/results_manager.c \
	$(SRC_DIR)/schedule.c \
	$(SRC_DIR)/sg.c \
	$(SRC_DIR)/signals.c \
	$(SRC_DIR)/target.c \
	$(SRC_DIR)/ticker.c \
	$(SRC_DIR)/timestamp.c \
	$(SRC_DIR)/utils.c \
	$(SRC_DIR)/verify.c \
	$(SRC_DIR)/xdd.c 

XDD_HEADERS = $(HDR_DIR)/access_pattern.h \
	$(HDR_DIR)/barrier.h \
	$(HDR_DIR)/end_to_end.h \
	$(HDR_DIR)/datapatterns.h \
	$(HDR_DIR)/misc.h \
	$(HDR_DIR)/parse.h \
	$(HDR_DIR)/pclk.h \
	$(HDR_DIR)/ptds.h \
	$(HDR_DIR)/read_after_write.h \
	$(HDR_DIR)/restart.h \
	$(HDR_DIR)/results.h \
	$(HDR_DIR)/sg.h \
	$(HDR_DIR)/sg_err.h \
	$(HDR_DIR)/sg_include.h \
	$(HDR_DIR)/ticker.h \
	$(HDR_DIR)/timestamp.h \
	$(HDR_DIR)/xdd.h \
	$(HDR_DIR)/xdd_common.h \
	$(HDR_DIR)/xdd_version.h

#
# Objects
#
XDD_OBJECTS = $(patsubst %.c, %.o, $(filter %.c, $(XDD_SOURCE)))
TS_OBJECTS = $(SRC_DIR)/timeserver.o $(SRC_DIR)/pclk.o $(SRC_DIR)/ticker.o
GT_OBJECTS = $(SRC_DIR)/gettime.o \
	$(SRC_DIR)/global_time.o \
	$(SRC_DIR)/pclk.o \
	$(SRC_DIR)/ticker.o

#
# Default build settings
#
CC = 		gcc
INCLUDES = -I$(SRC_DIR)
LIBRARIES =	-lpthread
CFLAGS = $(INCLUDES) -DXDD_VERSION=$(XDDVERSION) -DLINUX -O2 -g

#
# Platform specific build settings
#
$(info Making xdd for $(OS))
ifeq '$(OS)' 'Linux'
CFLAGS = $(INCLUDES) -DXDD_VERSION=$(XDDVERSION) -DLINUX -O2 -DSG_IO -D__INTEL__ -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -g -fno-strict-aliasing -Wall
endif
ifeq '$(OS)' 'Darwin' 
CFLAGS = $(INCLUDES) -DXDD_VERSION=$(XDDVERSION) -DOSX -O2 -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -g
OS = 		OSX
endif
ifeq '$(OS)' 'FreeBSD' 
CFLAGS = $(INCLUDES) -DXDD_VERSION=$(XDDVERSION) -DFREEBSD -O2 -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -g
endif
ifeq '$(OS)' 'Solaris' 
CC = 		cc
CFLAGS = $(INCLUDES) -DXDD_VERSION=$(XDDVERSION) -DSOLARIS -g
LIBRARIES =	-lsocket -lnsl -lpthread  -lxnet -lposix4 -v 
endif
ifeq '$(OS)' 'AIX'
CC =            xlc_r
CFLAGS = $(INCLUDES) -DXDD_VERSION=$(XDDVERSION) -DAIX -D_THREAD_SAFE -g -q64 -qcpluscmt -qthreaded
LIBRARIES = -lnsl -lpthread  -lxnet -v
endif

#
# Build rules
#
all:	xdd timeserver gettime

xdd: $(XDD_OBJECTS)
	$(CC) -o $@ $(CFLAGS) $^ $(LIBRARIES)
	mv -f $@ bin/xdd.$(OS)
	rm -f bin/xdd
	ln -s xdd.$(OS) bin/xdd

timeserver: $(TS_OBJECTS) 
	$(CC) -o $@ $(CFLAGS) $^ $(LIBRARIES)
	mv -f timeserver bin/timeserver.$(OS)
	rm -f bin/timeserver
	ln -s timeserver.$(OS) bin/timeserver

gettime: $(GT_OBJECTS) 
	$(CC)  -o $@ $(CFLAGS) $^ $(LIBRARIES)
	mv -f gettime bin/gettime.$(OS)
	rm -f bin/gettime
	ln -s gettime.$(OS) bin/gettime

baseversion:
	echo "#define XDD_BASE_VERSION $(XDDVERSION)" > xdd_base_version.h

dist:	clean baseversion tarball
		echo "Base Version $(XDDVERSION) Source Only"

fulldist:	clean baseversion all tarball
		echo "Base Version $(XDDVERSION) built"

tarball:
	tar cfz ../$(PROJECT).$(XDD_VERSION).tgz .

oclean:
	$(info Cleaning the $(OS) OBJECT files )
	rm -f $(XDD_OBJECTS) $(TS_OBJECTS) $(GT_OBJECTS)

clean: oclean
	$(info Cleaning the $(OS) executable files )
	rm -f bin/xdd.$(OS) \
		bin/xdd \
		bin/timeserver.$(OS) \
		bin/timeserver \
		bin/gettime.$(OS) \
		bin/gettime \
		a.out 
	rm -rf doc/doxygen

install: clean all fastinstall

fastinstall: all
	rm -f /sbin/xdd.$(OS) /sbin/xdd
	cp bin/xdd.$(OS) /sbin
	ln /sbin/xdd.$(OS) /sbin/xdd
	rm -f /sbin/timeserver.$(OS) /sbin/timeserver
	cp bin/timeserver.$(OS) /sbin
	ln /sbin/timeserver.$(OS) /sbin/timeserver
	rm -f /sbin/gettime.$(OS) /sbin/gettime
	cp bin/gettime.$(OS) /sbin
	ln /sbin/gettime.$(OS) /sbin/gettime

doc:
	doxygen doc/Doxyfile
	cd doc/doxygen/latex && make pdf

#
# Make meta-directives
#
.PHONY: all clean doc fastinstall install oclean tarball
