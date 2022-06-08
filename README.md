# dmenu - dynamic menu

dmenu is an efficient dynamic menu for X.

This is my build of it.

## Changes

This fork as several diffs from the suckless website (all of which can be found
in the `diffs` directory. Most changes are cosmetic.

This fork does not provide `stest`. Instead `dmenu_path` is a native executable.

## Requirements

**dmenu**

c compiler and `make`. Links against Xlib.

**dmenu_path**

D compiler (currently hard-coded as `dmd`). Links against Phobos.

## Installation

Edit config.mk to match your local setup (dmenu is installed into the /usr/local
namespace by default).

Afterwards enter the following command to build and install dmenu (if necessary
as root):

make clean install

## Running dmenu

See the man page for details.
