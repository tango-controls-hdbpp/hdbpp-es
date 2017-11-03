#-----------------------------------------
#	 Setup
#-----------------------------------------

NAME_SRV = hdb++es-srv
PREFIX=/usr/local

#-----------------------------------------
#	 Include Paths
#-----------------------------------------

ifdef TANGO_INC
	INC_DIR += -I${TANGO_INC}
endif

ifdef TANGO_LIB
	LIB_DIR	+= -L${TANGO_LIB}
endif

ifdef OMNIORB_INC
	INC_DIR	+= -I${OMNIORB_INC}
endif

ifdef OMNIORB_LIB
	LIB_DIR	+= -L${OMNIORB_LIB}
endif

ifdef ZMQ_INC
	INC_DIR += -I${ZMQ_INC}
endif

ifdef LIBHDBPP_INC
	INC_DIR += -I${LIBHDBPP_INC}
endif

ifdef LIBHDBPP_LIB
	LIB_DIR	+= -L${LIBHDBPP_LIB}
endif

ifdef ADDITIONAL_LIBS
	LIB_DIR	+= ${ADDITIONAL_LIBS}
endif

#-----------------------------------------
#	 Default make entry
#-----------------------------------------

default: release
release debug: bin/$(NAME_SRV)

#-----------------------------------------
#	Set CXXFLAGS and LDFLAGS
#-----------------------------------------

CXXFLAGS += -std=gnu++0x -D__linux__ -D__OSVERSION__=2 -pedantic -Wall \
	-Wno-non-virtual-dtor -Wno-long-long -DOMNI_UNLOADABLE_STUBS $(INC_DIR) -Isrc

ifeq ($(GCCMAJOR),4)
    CXXFLAGS += -Wextra
endif
ifeq ($(GCCMAJOR),5)
    CXXFLAGS += -Wextra
endif

LDFLAGS += $(LIB_DIR) -lhdb++ -ltango -llog4tango -lomniORB4 -lomniDynamic4 -lomnithread

#-----------------------------------------
#	Set dependencies
#-----------------------------------------

SRC_FILES += $(wildcard src/*.cpp)
OBJ_FILES += $(addprefix obj/,$(notdir $(SRC_FILES:.cpp=.o)))

obj/%.o: $(SRC_FILES:%.cpp)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

.nse_depinfo: $(SRC_FILES)
	@$(CXX) $(CXXFLAGS) -M -MM $^ | sed 's/\(.*\)\.o/obj\/\1.o/g' > $@
-include .nse_depinfo

#-----------------------------------------
#	 Main make entries
#-----------------------------------------

bin/$(NAME_SRV): bin obj $(OBJ_FILES) 
	$(CXX) $(CXXFLAGS) $(OBJ_FILES) -o bin/$(NAME_SRV) $(LDFLAGS)

clean:
	@rm -fr obj/ bin/ core* .nse_depinfo src/*~

bin obj:
	@ test -d $@ || mkdir $@

install:
	install -d ${DESTDIR}$(PREFIX)/bin
	install -m 755 bin/hdb++es-srv ${DESTDIR}$(PREFIX)/bin

#-----------------------------------------
#	 Target specific options
#-----------------------------------------

release: CXXFLAGS += -O2 -DNDEBUG
release: LDFLAGS += -s
debug: CXXFLAGS += -ggdb3

.PHONY: clean
