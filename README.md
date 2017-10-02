# Hdbpp-es

Tango device server for the HDB++ Event Subscriber 

## Version

The current release version is 1.0.0

## Documentation

* See the Tango documentation [here](http://tango-controls.readthedocs.io/en/latest/administration/services/hdbpp/index.html#hdb-an-archiving-historian-service) for broader information about the HB++ archiving system and its integration into Tango Controls
* Hdbpp-es [CHANGELOG.md](https://github.com/tango-controls/libhdbpp/blob/master/CHANGELOG.md) contains the latest changes both released and in development.

## Building

To get the source, pull from git:

```bash
git clone http://github.com/tango-controls/hdbpp-es.git  
cd hdbpp-es
```

Set appropriate flags in the environment (or pass them to make) if required, then:

```bash
make
```

### Build Flags

There are a set of library and include variables that can be set to inform the build of various dependencies. The flags are only required if you have installed a dependency in a non-standard location. 

| Flag | Notes |
|------|-------|
| TANGO_INC | Tango include files directory |
| TANGO_LIB | Tango lib files directory |
| OMNIORB_LIB | Omniorb lib files directory |
| LIBHDBPP_INC | Libhdb++ include files directory
| LIBHDBPP_LIB | Libhdb++ lib files directory |
