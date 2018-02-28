# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/en/1.0.0/)
and this project adheres to [Semantic Versioning](http://semver.org/spec/v2.0.0.html).

## [Unreleased]

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
