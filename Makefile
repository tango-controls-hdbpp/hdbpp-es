NAME_SRV = hdb++es-srv

LIBHDBPP_DIR = .libhdbpp
LIBHDBPP_INC = ./$(LIBHDBPP_DIR)/src
LIBHDBPP_LIB = ./$(LIBHDBPP_DIR)/lib
MAKE_INC = .hdbpp-common

CXXFLAGS += -DRELEASE='"$HeadURL$ "' -I./$(LIBHDBPP_INC)
LDFLAGS = -lhdb++ -L./$(LIBHDBPP_LIB)

include ./$(MAKE_INC)/Make-hdbpp.in

#-----------------------------------------
#	 Default make entry
#-----------------------------------------
default: release
release debug: bin/$(NAME_SRV)

#-----------------------------------------
#	Set CXXFLAGS and LDFLAGS
#-----------------------------------------
CXXFLAGS += -std=gnu++0x -D__linux__ -D__OSVERSION__=2 -pedantic -Wall \
	-Wno-non-virtual-dtor -Wno-long-long -DOMNI_UNLOADABLE_STUBS \
	$(INC_DIR) -Isrc
ifeq ($(GCCMAJOR),4)
    CXXFLAGS += -Wextra
endif
ifeq ($(GCCMAJOR),5)
    CXXFLAGS += -Wextra
endif
LDFLAGS += $(LIB_DIR) -ltango -llog4tango -lomniORB4 -lomniDynamic4 \
	-lCOS4 -lomnithread -lzmq

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
	$(MAKE) -C $(LIBHDBPP_DIR)
	$(CXX) $(CXXFLAGS) $(OBJ_FILES) -o bin/$(NAME_SRV) $(LDFLAGS)

clean:
	$(MAKE) -C $(LIBHDBPP_DIR) clean
	@rm -fr obj/ bin/ core* .nse_depinfo src/*~

bin obj:
	@ test -d $@ || mkdir $@

#-----------------------------------------
#	 Target specific options
#-----------------------------------------
release: CXXFLAGS += -O2 -DNDEBUG
release: LDFLAGS += -s
debug: CXXFLAGS += -ggdb3

.PHONY: clean
