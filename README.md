# hdbpp-es

[![TangoControls](https://img.shields.io/badge/-Tango--Controls-7ABB45.svg?style=flat&logo=%20data%3Aimage%2Fpng%3Bbase64%2CiVBORw0KGgoAAAANSUhEUgAAACAAAAAkCAYAAADo6zjiAAAABHNCSVQICAgIfAhkiAAAAAlwSFlzAAALEwAACxMBAJqcGAAAAsFJREFUWIXtl01IFVEYht9zU%2FvTqOxShLowlOgHykWUGEjUKqiocB1FQURB0KJaRdGiaFM7gzZRLWpTq2olhNQyCtpYCP1gNyIoUTFNnxZzRs8dzvw4Q6564XLnfOf73vedc2a%2BmZEKALgHrC3CUUR8CxZFeEoFalsdM4uLmMgFoIlZLJp3A9ZE4S2oKehhlaR1BTnyg2ocnW%2FxsxEDhbYij4EPVncaeASMAavnS%2FwA8NMaqACNQCew3f4as3KZOYh2SuqTVJeQNiFpn6QGSRVjTH9W%2FiThvcCn6H6n4BvQDvQWFT%2BSIDIFDAKfE3KOAQeBfB0XGPeQvgE67P8ZoB44DvTHmFgJdOQRv%2BUjc%2BavA9siNTWemgfA3TwGquCZ3w8szFIL1ALngIZorndvgJOR0GlP2gtJkzH%2Bd0fGFxW07NqY%2FCrx5QRXcYjbCbmxF1dkBSbi8kpACah3Yi2Sys74cVyxMWY6bk5BTwgRe%2BYlSzLmxNpU3aBeJogk4XWWpJKUeiap3RJYCpQj4QWZDQCuyIAk19Auj%2BAFYGZZjTGjksaBESB8P9iaxUBIaJzjZcCQcwHdj%2BS2Al0xPOeBYYKHk4vfmQ3Y8YkIwRUb7wQGU7j2ePrA1URx93ayd8UpD8klyPbSQfCOMIO05MbI%2BDvwBbjsMdGTwlX21AAMZzEerkaI9zFkP4AeYCPBg6gNuEb6I%2FthFgN1KSQupqzoRELOSed4DGiJala1UmOMr2U%2Bl%2FTWEy9Japa%2Fy41IWi%2FJ3d4%2FkkaAw0Bz3AocArqApwTvet3O3GbgV8qqjAM7bf4N4KMztwTodcYVyelywKSCD5V3xphNXoezuTskNSl4bgxJ6jPGVJJqbN0aSV%2Bd0M0aO7FCs19Jo2lExphXaTkxdRVgQFK7DZVDZ8%2BcpdmQh3wuILh7ut3AEyt%2B51%2BL%2F0cUfwFOX0t0StltmQAAAABJRU5ErkJggg%3D%3D)](http://www.tango-controls.org) [![License: GPL v3](https://img.shields.io/badge/License-GPL%20v3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0) [![](https://img.shields.io/github/release/tango-controls-hdbpp/hdbpp-es.svg)](https://github.com/tango-controls-hdbpp/hdbpp-es/releases) [![Download](https://api.bintray.com/packages/tango-controls/debian/hdb%2B%2Bes/images/download.svg)](https://bintray.com/tango-controls/debian/hdb%2B%2Bes/_latestVersion)

Tango device server for the HDB++ Event Subscriber 

## Documentation

* See the Tango documentation [here](http://tango-controls.readthedocs.io/en/latest/administration/services/hdbpp/index.html#hdb-an-archiving-historian-service) for broader information about the HB++ archiving system and its integration into Tango Controls
* hdbpp-es [CHANGELOG.md](https://github.com/tango-controls-hdbpp/hdbpp-es/blob/master/CHANGELOG.md) contains the latest changes both released and in development.

## Bugs Reports

Please file bug reports above in the issues section.

## Building and Installation

In its simplest form, clone the repository and assuming a standard install for all dependencies:

```
cd hdbpp-es
mkdir build
cd build
cmake ../
make
make install
```

For more complex build, if you want cmake to fetch the dependencies, or to link against a specific backend, [see the examples](#custom-builds-examples)

### pkg-config Settings

The build system uses pkg-config to find some dependencies, for example Tango. If Tango is not installed to a standard location, set PKG_CONFIG_PATH, i.e.

```bash
export PKG_CONFIG_PATH=/non/standard/tango/install/location
...
cmake ../
...
```

The pkg-config path can also be set with the cmake argument CMAKE_PREFIX_PATH. This can be set on the command line at configuration time, i.e.:

```bash
...
cmake -DCMAKE_PREFIX_PATH=/non/standard/tango/install/location ../
...
```

### Build Dependencies

Build dependencies:

* libhdbpp version 2 or greater. Not compatible with any version below this.

The build requires the libhdbpp project headers for building. There is two methods to include this dependency:

* libhdbpp can be installed in either a standard or non-standard location and cmake flags used to locate it.
* Enabled the custom cmake flag FETCH_LIBHDBPP ([see below](#project-flags)) and cmake will download the project at configuration time and integrate it directly into the build. This is ideal for testing the project in isolation from a deployed system.

Ensure the development version of the dependencies are installed. These are as follows:

* Tango Controls 9 or higher development headers and libraries
* omniORB release 4 or higher development headers and libraries
* libzmq3-dev or libzmq5-dev

If they have not been installed to a standard location, then use the pkg-config settings above to inform the build where they are located.

### Toolchain Dependencies

If wishing to build the project, ensure the following dependencies are met:

* CMake 3.6 or higher
* C++14 compatible compiler

### Project Flags

| Flag | Setting | Default | Description |
|------|-----|-----|-----|
| ENABLE_CLANG | ON/OFF | OFF | Clang code static analysis and cppcore guideline enforcement |
| FETCH_LIBHDBPP | ON/OFF | OFF | Download and build against libhdbpp locally |
| LIBHDBPP_BACKEND | "backend_name" | "libhdbpp" | Backend to use for this build. Supported values are libhdbpp, timescaledb, mysql, cassandra. |
| FETCH_LIBHDBPP_TAG | tag/branch | master | When FETCH_LIBHDBPP is enabled, this flag defines the tag/branch to download |

### Standard CMake Flags

The build system is CMake therefore standard CMake flags can be used to influence the build and installation process. The following is a list of common useful CMake flags and their use:

| Flag | Use |
|------|-----|
| CMAKE_INSTALL_PREFIX | Standard CMake flag to modify the install prefix. |
| CMAKE_INCLUDE_PATH | Standard CMake flag to add include paths to the search path. |
| CMAKE_LIBRARY_PATH | Standard CMake flag to add paths to the library search path |

### Custom builds examples

Here are some examples to demonstrate the use of the project flags.

In all the examples, the backends need to implement libhbpp v2.x at least and set the proper cmake variable as explained in libhdbpp example.

If you want to build and install your backend directly while building this project you can do:

```
cd hdbpp-es
mkdir build
cd build
cmake -DFETCH_LIBHDBPP=ON ../
make
make install
```

To fetch and build a specific backend you can use the LIBHDBPP_BACKEND flag to specify wich backend to use. The cmake command will look like (for timescaledb):

```
cmake -DFETCH_LIBHDBPP=ON -DLIBHDBPP_BACKEND=timescaledb ../
```

Finally, if you want to use a specific backend you already have installed, you don't need to fetch it. The cmake command will look like (for mysql):

```
cmake -DLIBHDBPP_BACKEND=mysql ../
```

## License

The code is released under the GPL3 license and a copy of this license is provided with the code.
