USRP Hardware Driver (UHD™) Software
========================================

Welcome to the UHD™ software distribution! UHD is the free & open-source
software driver and API for the Universal Software Radio Peripheral (USRP™) SDR
platform, created and sold by Ettus Research.

UHD supports all Ettus Research USRP™ hardware, including all motherboards and
daughterboards, and the combinations thereof.

## Documentation

The [UHD Homepage](http://code.ettus.com/redmine/ettus/projects/uhd/wiki),
through which you can find installation instructions, documentation, and further
information, is the primary source of UHD documentation and "Getting
Started"-type guides.

For technical documentation related to USRP™ hardware or UHD system
design, check out the [UHD and USRP Manual](http://files.ettus.com/manual/).

Additionally, be sure to check out the Ettus Research
[FAQ](http://www.ettus.com/kb/detail/frequently-asked-questions), and the
[Knowledge Base](http://www.ettus.com/kb) for useful application notes and
tutorials.

## OS Support

UHD is primarily developed on Linux, but we also test and support the following
operating systems.

* Linux (any distribution)
* Mac OS X (PPC and Intel)
* Windows 7/Vista/XP

## Applications

UHD can be used to build stand-alone applications with USRP™ hardware, or with
third-party applications. Some common toolkits / frameworks are:

* [GNURadio](http://code.ettus.com/redmine/ettus/projects/uhd/wiki/GNU_Radio_UHD)
* [LabVIEW](http://www.ni.com/download/ni-usrp-1.3/4711/en/)
* [Simulink](http://www.mathworks.com/discovery/sdr/usrp.html)
* [OpenBTS](http://wush.net/trac/rangepublic/wiki/BuildInstallRun)
* [Iris](http://www.softwareradiosystems.com/redmine/projects/iris/wiki)
* [Redhawk](https://github.com/redhawksdr)
* [Amarisoft LTE eNodeB](http://www.amarisoft.com/?p=amarilte)

## Directories

__host/__

The source code for the user-space driver.

__firmware/__

The source code for all microprocessors in USRP hardware.

__fpga-src/__

The source code for the UHD FPGA images. Note this is a git submodule,
if you are cloning the repository and want to modify the FPGA code,
you will need to run 'git clone --recursive' to automatically
populate this directory. Alternatively, you can run 'git submodule init'
followed by 'git submodule update' to populate it after cloning the
repository without '--recursive'.

Note that this subdirectory is very large, and not necessary for
building applications that link against UHD.

__images/__

This contains the package builder for FPGA and firmware images.
We provide other tools to downloade image packages, the scripts in here
are mainly relevant for UHD maintainers and -developers.

__tools/__

Additional tools, mainly for debugging purposes. See the readme-file
in that directory for more details on the individual tools.

