.PHONY: all clean install tests

include config.mk

CC = gcc
CFLAGS ?= -g3 -ggdb -O3 -Wall -Wextra # -qopt-report=4 -qopt-report-phase ipo
AR ?= ar
RANLIB ?= ranlib
PREFIX ?= /usr

SRCDIR = ./src
OBJDIR ?= ./obj

EXTRA = -DAFF_DEBUG -DUSE_AFFI #-DUSE_MINI_PSM2# -DUSE_ABT

ifeq (,$(wildcard $(OBJDIR)))
$(shell mkdir -p $(OBJDIR))
endif

ifeq (,$(wildcard $(OBJDIR)/pmi))
$(shell mkdir -p $(OBJDIR)/pmi)
endif

ifeq (,$(wildcard $(OBJDIR)/dreg))
$(shell mkdir -p $(OBJDIR)/dreg)
endif

INCLUDE = -I./include -I./src/include -I./  -I./src/
CFLAGS += -fPIC -fvisibility=hidden -std=gnu99 $(INCLUDE) $(EXTRA) -D_GNU_SOURCE -pthread

IBV_DIR = /opt/ofed/
PSM_DIR = $(HOME)/libpsm2/usr/

CFLAGS += $(IBV_INC)
LDFLAGS += -shared -Lstatic

### FOR USING PAPI ##
# PAPI_LIB = $(TACC_PAPI_LIB) -lpapi
# PAPI_INC = $(TACC_PAPI_INC)
# CFLAGS += -I$(PAPI_INC)
# LDFLAGS += -L$(PAPI_LIB)

ifeq ($(LC_SERVER_DEBUG), yes)
CFLAGS += -DLC_SERVER_DEBUG
endif

ifeq ($(LC_SERVER_INLINE), yes)
CFLAGS += -DLC_SERVER_INLINE
endif

ifeq ($(LC_SERVER), ofi)
	CFLAGS += -DLC_USE_SERVER_OFI -DAFF_DEBUG
endif

ifeq ($(LC_SERVER), ibv)
	CFLAGS += -DLC_USE_SERVER_IBV -DAFF_DEBUG -I$(IBV_DIR)/include
endif

ifeq ($(LC_SERVER), psm)
	CFLAGS += -DLC_USE_SERVER_PSM -DAFF_DEBUG -I$(PSM_DIR)/include
endif

COMM = lc.o medium.o short.o long.o tag.o cq.o misc.o ep.o lcrq.o pool.o hashtable.o coll.o
DREG = dreg/dreg.o dreg/avl.o
PMI = pm.o pmi/simple_pmi.o pmi/simple_pmiutil.o

ifeq ($(LC_USE_DREG), yes)
LIBOBJ += $(addprefix $(OBJDIR)/, $(DREG))
CFLAGS += -DUSE_DREG
endif

OBJECTS = $(addprefix $(OBJDIR)/, $(COMM))

LIBOBJ += $(OBJECTS) $(addprefix $(OBJDIR)/, $(FCONTEXT)) $(addprefix $(OBJDIR)/, $(PMI))

LIBRARY = liblci.so
ARCHIVE = liblci.a

all: $(LIBRARY) $(ARCHIVE)

install: all
	mkdir -p $(PREFIX)/bin
	mkdir -p $(PREFIX)/lib
	mkdir -p $(PREFIX)/include
	cp lcrun $(PREFIX)/bin
	cp -R include/* $(PREFIX)/include
	cp liblwci.a $(PREFIX)/lib
	cp liblwci.so $(PREFIX)/lib

$(OBJDIR)/%.o: $(SRCDIR)/$(notdir %.c)
	$(CC) $(CFLAGS) -c $< -o $@

$(LIBRARY): $(LIBOBJ)
	$(CC) $(LDFLAGS) $(LIBOBJ) $(MALLOC) -o $(LIBRARY)

$(ARCHIVE): $(LIBOBJ)
	@rm -f $(ARCHIVE)
	$(AR) q $(ARCHIVE) $(LIBOBJ) $(MALLOC)
	$(RANLIB) $(ARCHIVE)

mpiv.a: obj/mpiv.o
	$(AR) q mpiv.a obj/mpiv.o $(LIBOBJ)
	$(RANLIB) mpiv.a

clean:
	rm -rf $(LIBOBJ) $(OBJDIR)/* liblwci.a liblwci.so mpiv.a

tests:
	$(MAKE) -C tests && ./tests/all_test
