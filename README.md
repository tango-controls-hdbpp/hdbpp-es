# Hdbpp-es

Tango device server for the HDB++ Event Subscriber 

## Documentation

* See the Tango documentation [here](http://tango-controls.readthedocs.io/en/latest/administration/services/hdbpp/index.html#hdb-an-archiving-historian-service) for broader information about the HB++ archiving system and its integration into Tango Controls
* Libhdbpp [CHANGELOG.md](https://github.com/tango-controls/libhdbpp/blob/master/CHANGELOG.md) contains the latest changes both released and in development.

## Building

To get the source, pull from git:

```bash
git clone http://github.com/tango-controls/hdbpp-es.git  
cd hdbpp-es
```

Set appropriate flags in the environment (or pass them to make) if required, then:

```bashv
make
```

### Build Flags

There are two sets of flags to assist in building the binary. Only use one set for each dependency. The flags are only required if you have installed a dependency in a non-standard location. 

The first define the install location of a dependency:

| Flag | Notes |
|------|-------|
| TANGO_DIR | Tango install root |
| OMNIORB_DIR | Omniorb install root |
| LIBHDBPP_DIR | Libhdb++ install root |

The above take priority in the build over the following, which define the include and lib directory for each dependency: 

| Flag | Notes |
|------|-------|
| TANGO_INC | Tango include files directory |
| TANGO_LIB | Tango lib files directory |
| OMNIORB_INC | Omniorb include files directory
| OMNIORB_LIB | Omniorb lib files directory |
| LIBHDBPP_INC | Libhdb++ include files directory
| LIBHDBPP_LIB | Libhdb++ lib files directory |
