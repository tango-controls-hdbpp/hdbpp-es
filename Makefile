NAME_SRV = hdb++es-srv

include ../../Make-hdb++.in

CXXFLAGS += -DRELEASE='"N$Name:  $ "' -I$(LIBHDB_INC)
LDFLAGS = -lhdb++ -l$(LIBHDBIMPL) -l$(SQLIMPL) -L$(LIBHDB_LIB) -L$(LIBHDBIMPL_LIB) -L$(SQLIMPL_LIB)

include ../../Make-8.1.2.c.in
