# Hdbpp-es

Tango device server for the HDB++ Event Subscriber 

## Version

The current release version is 1.0.0

## Documentation

* See the Tango documentation [here](http://tango-controls.readthedocs.io/en/latest/administration/services/hdbpp/index.html#hdb-an-archiving-historian-service) for broader information about the HB++ archiving system and its integration into Tango Controls
* hdbpp-es [CHANGELOG.md](https://github.com/tango-controls/libhdbpp/blob/master/CHANGELOG.md) contains the latest changes both released and in development.

## Building

### Dependencies

Ensure the development version of the dependencies are installed. These are as follows:

* HDB++ library libhdbpp
* Tango Controls 9.2.5a - either via debian package or source install.
* omniORB release 4 - libomniorb4 and libomnithread
* libzmq - libzmq3-dev or libzmq5-dev

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
git clone http://github.com/tango-controls/hdbpp-es.git  
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

The code is released under the LGPL3 license and a copy of this license is provided with the code. 
