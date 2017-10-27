# Hdbpp-es

Tango device server for the HDB++ Event Subscriber 

## Version

The current release version is 1.0.0

### **Important Changes** 0.9.0 -> 1.0.0

* The include file LibHdb++.h has changed from `include "LibHdb++.h"` to `include "libhdb++/LibHdb++.h"`. This reflects the standard install path of the headers. If building against [libhdbpp](https://github.com/tango-controls-hdbpp/libhdbpp) from a standard system install, then this change makes no difference. If libhdbpp is installed to a custom location, ensure you add the path to the CMAKE_INCLUDE_PATH.

## Documentation

* See the Tango documentation [here](http://tango-controls.readthedocs.io/en/latest/administration/services/hdbpp/index.html#hdb-an-archiving-historian-service) for broader information about the HB++ archiving system and its integration into Tango Controls
* hdbpp-es [CHANGELOG.md](https://github.com/tango-controls-hdbpp/libhdbpp/blob/master/CHANGELOG.md) contains the latest changes both released and in development.

## Building

### Dependencies

Ensure the development version of the dependencies are installed. These are as follows:

* HDB++ library libhdbpp.
* Tango Controls 9 or higher.
* omniORB release 4 - libomniorb4 and libomnithread.
* libzmq - libzmq3-dev or libzmq5-dev.

#### **Tango 8**

It is also possible to compile and link against tango 8. This is discouraged since this device server has not been test built and run against tango 8 recently. Offically we support tango 9 onwards.

### Build Flags

There are a set of library and include variables that can be set to inform the build of various dependencies. The flags are only required if you have installed a dependency in a non-standard location. 

| Flag | Notes |
|------|-------|
| TANGO_INC | Tango include files directory |
| TANGO_LIB | Tango lib files directory |
| OMNIORB_INC | Omniorb include files directory |
| OMNIORB_LIB | Omniorb lib files directory |
| ZMQ_INC | ZMQ include files directory |
| LIBHDBPP_INC | Libhdb++ include files directory |
| LIBHDBPP_LIB | Libhdb++ lib files directory |

### Build

To get the source, pull from git:

```bash
git clone http://github.com/tango-controls-hdbpp/hdbpp-es.git  
cd hdbpp-es
```

Set appropriate flags in the environment (or pass them to make) if required, then:

```bash
make
```

## Installation

Once built simply run `make install`. The install can be passed a PREFIX variable, this is set to /usr/local by default. It is also possible to use DESTDIR. Install path is constructed as DESTDIR/PREFIX.

#### Building Against Tango Controls 9.2.5a

**The debian package and source install place the headers under /usr/include/tango, so its likely you will need to set TANGO_INC=/usr/include/tango or TANGO_INC=/usr/local/include/tango, depending on your install method.**

## License

The code is released under the GPL3 license and a copy of this license is provided with the code. 
