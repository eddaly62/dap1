# Makefile for DAP
#
# Usage:
# make and make all - build everything for ARM target, debug config.
# make lib - build DAP library for ARM target, debug config.
# make graph - build doc files
# make release - build everything, ARM target, no debug or gdb support, release config.
# make local - build everything, local target (X86), debug config.
# make clean - remove all generated files.
# make allscp - same as 'make all' and "make" and copies test builds to remote target
# make first - run this the first time you checkout the project
# make help - displays make options
#
# Notes:
# 1) May need to change the XPREFIX and XPATH to use the target dev tool set,
#    change the path to were you located the tool set of your target.
# 2) May need to change TLIBS to use the c librarys of your target,
#    path to iso image of your target. You need this foir your test app builds.
# 3) Will need to change HOST and TARGET with you targets specifics.
#    These settings support copying your test apps to the target
# 4) To build graph docs you will need to have graphviz installed.
#    To install graphviz:
#       sudo apt install graphviz
# 5) Naming Rules:
#    a. All DAP files begin with a dap prefix and end with a .c or .h.
#    b. All test files begin with test prefix and end with a .c
#    c. All graph files end with a .dot

XPREFIX=arm-none-linux-gnueabihf-
XPATH=/home/gdc419/gcc-linaro/bin
CC=$(XPATH)/$(XPREFIX)gcc
AR=$(XPATH)/$(XPREFIX)ar
CFLAGS=-Wall -O0 -g -DDEBUG
AFLAGS=-cvrs

# Library build
BIN=dap
LSRC=src
LSRCS=$(wildcard $(LSRC)/dap*.c)
LOBJ=obj
LOBJS=$(patsubst $(LSRC)/%.c, $(LOBJ)/%.o, $(LSRCS))
LIBDIR=bin
LIB=$(LIBDIR)/$(BIN).a

# Test builds
TSRC=src
TSRCS=$(wildcard $(TSRC)/test*.c)
TBIN=bin
TBINS=$(patsubst $(TSRC)/%.c, $(TBIN)/%, $(TSRCS))
TLIBS =-L/srv/nfs/Apalis-iMX6_Console-Image-Tezi_3.0b4.254/lib/ -lpthread bin/dap.a

HOST=~/Projects/$(BIN)
TARGET=root@192.168.20.64:/home/root/
CTT=scp
CP=off

# Graph file build
# Options for GFLAGS: for pdf output use Tpdf, for jpg output use Tjpg
GSRC=graph
GSRCS=$(wildcard $(GSRC)/*.dot)
GG=dot
GFLAGS=-Tsvg
GEXT=svg
GBINS=$(patsubst $(GSRC)/%.dot, $(GSRC)/%.$(GEXT), $(GSRCS))


all: lib test graph
allscp: CP=on
allscp: lib test graph
release: CFLAGS=-Wall -O0
release: clean lib test graph
local: CC=gcc
local: AR=ar
local: TLIBS=bin/dap.a -lpthread
local: lib test graph

lib: $(LIB)

$(LIB): $(LOBJS)
	@echo "\e[1mBuilding DAP library\e[0m"
	$(AR) $(AFLAGS) $@ $(LOBJS)
	file $@

$(LOBJ)/%.o: $(LSRC)/%.c
	@echo "\e[1mCompiling $< \e[0m"
	$(CC) $(CFLAGS) -c $< -o $@


test: $(TBINS)

$(TBIN)/%: $(TSRC)/%.c $(LIB)
	@echo "\e[1mBuilding Test program $< \e[0m"
	$(CC) $(CFLAGS) $< -o $@ $(TLIBS)
	$(if $(findstring on,$(CP)), $(CTT) $(HOST)/$@ $(TARGET), @echo "$@ not copied to remote")

# creates doc files
# will need to install graphviz
# sudo apt install graphviz
graph: $(GBINS)

$(GSRC)/%.$(GEXT): $(GSRC)/%.dot
	@echo "\e[1mBuilding Graph $< \e[0m"
	$(GG) $< $(GFLAGS) > $@

clean:
	@echo "\e[1mClean everything\e[0m"
	$(RM) -r obj/*
	$(RM) -r bin/*
	$(RM) -r graph/*.$(GEXT)

help:
	@echo "\e[1mUsage:\e[0m"
	@echo "make and make all - build everything for ARM target, debug config."
	@echo "make lib - build DAP library for ARM target, debug config"
	@echo "make graph - build doc files"
	@echo "make release - build everything, ARM target, no debug or gdb support, release config."
	@echo "make local - build everything, local x86 target, debug config."
	@echo "make clean - remove all generated files."
	@echo "make allscp - same as 'make all' and "make" and copies test builds to remote target"
	@echo "make first - run this the first time you checkout the project"

first:
	mkdir -p obj
	mkdir -p bin
