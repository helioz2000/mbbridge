#
# Makefile
#
BIN = mbbridge
BINDIR = /usr/local/sbin/
DESTDIR = /usr
PREFIX = /local
SERVICE = mbbridge.service
SERVICEDIR = /etc/systemd/system

# modbus header files could be located in different directories
INC += -I$(DESTDIR)/include/modbuspp -I$(DESTDIR)/include/modbus
INC += -I$(DESTDIR)$(PREFIX)/include/modbuspp -I$(DESTDIR)$(PREFIX)/include/modbus

CC=gcc
CXX=g++
CFLAGS = -Wall -Wshadow -Wundef -Wmaybe-uninitialized -Wno-unknown-pragmas $(INC) 
# enable optimizing for release:
CFLAGS += -O3
#enable debug symbols:
#CFLAGS += -g

# directory for local libs
LDFLAGS = -L$(DESTDIR)$(PREFIX)/lib
LIBS += -lstdc++ -lm -lmosquitto -lconfig++ -lmodbus

#VPATH =

#$(info LDFLAGS ="$(LDFLAGS)")
#$(info INC="$(INC)")

# folder for our object files
OBJDIR = ./obj

CSRCS += $(wildcard *.c)
CPPSRCS += $(wildcard *.cpp)

COBJS = $(patsubst %.c,$(OBJDIR)/%.o,$(CSRCS))
CPPOBJS = $(patsubst %.cpp,$(OBJDIR)/%.o,$(CPPSRCS))

SRCS = $(CSRCS) $(CPPSRCS)
OBJS = $(COBJS) $(CPPOBJS)

#.PHONY: clean

all: default

$(OBJDIR)/%.o: %.c
	@mkdir -p $(OBJDIR)
	@echo "CC $<"
	@$(CC)  $(CFLAGS) -c $< -o $@

$(OBJDIR)/%.o: %.cpp
	@mkdir -p $(OBJDIR)
	@echo "CXX $<"
	@$(CXX)  $(CFLAGS) -c $< -o $@

default: $(OBJS)
	$(CC) -o $(BIN) $(OBJS) $(LDFLAGS) $(LIBS)

#	nothing to do but will print info
nothing:
	$(info OBJS ="$(OBJS)")
	$(info DONE)


clean:
	rm -f $(OBJS)

install:
ifneq ($(shell id -u), 0)
	@echo "!!!! install requires root !!!!"
else
	install -o root $(BIN) $(BINDIR)$(BIN)
	@echo ++++++++++++++++++++++++++++++++++++++++++++
	@echo ++ $(BIN) has been installed in $(BINDIR)
	@echo ++ systemctl start $(BIN)
	@echo ++ systemctl stop $(BIN)
endif

# make systemd service
service:
	install -m644 -o root $(SERVICE) $(SERVICEDIR)
	@systemctl daemon-reload
	@systemctl enable mbbridge.service
	@echo $(BIN) is now available a systemd service
