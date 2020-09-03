# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/en/1.0.0/)
and this project adheres to [Semantic Versioning](http://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

* Build flag to choose which backend to build again.
* Update build mechanism to fetch libhdbpp implementation.
* DB_ADD command support to register an attribute in the db from the AttributeAdd command.
* New CMake build system that can download libhdbpp when requested
* Moved HDBCmdData source from libhdbpp project
* Clang integration

### Changed

* Do not depend on HdbClient.h, but only AbstracDB.h
* Small refactoring and code modernization to get rid of some clang warnings.
* Updated README for new build system
* Observe new namespace in libhdbpp
* Changed libhdbpp includes to new path (hdb++) and new split header files
* Project now links directly to given libhdbpp soname major version
* Made compatible with new libhdbpp (namespace, function and path changes)

## [1.0.2] - 2019-09-23

### Added

* Subscribe to change event as fallback.

## [1.0.1] - 2018-02-28

### Fixed

* Segmentation fault when error updating ttl.

## [1.0.0] - 2017-09-28

### Added

* CHANGELOG.md file.
* Debian Package build files under debian/

### Changed

* Makefile: Added install rules, clean up paths etc
* Makefile: Cleaned up the linkage (removed unneeded libraries, libzmq, libCOS)
* Cleaned up some unused variable warnings
* libhdb++ include paths to match new install location.
* Updated README.md.

### Removed

* libhdbpp submodule. This now has to be installed manually.
