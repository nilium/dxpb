dxpb(7) - Miscellaneous Information Manual

# NAME

**dxpb** - Builders and organization for running xbps-src builds well

# SYNOPSIS

**progname**
\[**-options**]
*file&nbsp;...*

# DESCRIPTION

The
**dxpb**
tool is a monolith program split into many binaries. These binaries can, and
probably should, be run under separate users. These binaries internally follow
certain "chains". Only binaries in the same "chain" can be connected to the
same endpoints.
**dxpb**
also requires certain resources.

The multiple program chains are as follows: Import, File, Graph.

# CHAINS

## Import

This chain is responsible for reading in xbps-src templates, understanding
what is set in every template, and getting the information needed to track
which packages should be build before building which others.

This set of programs can be aware of the full set of variables available in
an xbps-src template. Here there are the workers who import packages. These are
simple binaries, but are split out into separate binaries to prevent perceived
thread-unsafe file descriptor manipulations when forking().

This chain is where packages are read in for the grapher's sake, and where
the dxpb system is alerted to new packages.

Binaries are named dxpb-poke,
dxpb-pkgimport-agent, dxpb-pkgimport-master, and dxpb-grapher.

## File

This chain is responsible for xbps packages.
Here, files are identified by a triplet of pkgname, version, and arch.
There is support for transporting large binary files (far larger than 2
gigabytes) from remote workers to the main repository. This chain
exists to keep track of where files are.

Binaries are named dxpb-hostdir-master, dxpb-hostdir-remote and dxpb-grapher.

## Graph

On this chain, the graph of all packages is already known, and work is done to
realize the packages on that graph (do the actual building). Here the atom
being communicated is a worker which can help with a pair of target and host
architectures.

Binaries are named dxpb-hostdir-master, dxpb-frontend, dxpb-grapher, and
dxpb-builder.

# RESOURCES

There are a variety of resources needed by dxpb, and they are listed below.

## Import chain

*	The package database, owned and handled by the dxpb-grapher.

*	A git clone of the packages repository, owned and handled by the
	dxpb-pkgimport-master, but read from by the dxpb-pkgimport-agents.

*	An endpoint over which to communicate. See dxpb-grapher -h for the default
	endpoint.

## File chain

*	A directory which is the master repository. This will be owned and managed by
	the dxpb-hostdir-master daemon.

*	A directory for being owned and managed by the dxpb-hostdir-master daemon, for
	use as a staging directory, so as not to pollute the master repository with
	unfinished transfers.

*	A hostdir repository to be read from by any given dxpb-hostdir-remote. There
	should be a one-to-one mapping of these directories and daemons.

*	An endpoint over which to communicate. See dxpb-grapher -h for the default
	endpoint.

## Graph chain

*	A directory for package logs. This will be owned and managed by the
	dxpb-hostdir-master daemon. Build output per architecture/pkgname/version will
	be stored here.

*	A git-clone of a packages repository to be owned and managed by a single
	dxpb-builder process. It will do its job in this directory.

*	An endpoint over which to communicate. See dxpb-grapher -h for the default
	endpoint.

# AUTHORS

Toyam Cox &lt;Vaelatern@gmail.com&gt;

# BUGS

Plenty. We just haven't found them all yet.

# SECURITY CONSIDERATIONS

The dxpb-frontend is a rather dumb component. Almost everything goes directly
to the grapher, but is processed by the frontend first. The only reason for
this is to avoid exposing the grapher directly to the internet, since the
grapher actually is capable of ordering builds.

The hostdir-master is NOT a dumb endpoint. Exposing a vulnerability in this
program means exposing the entire repository to an attacker. In the future this
might be fixed.

# SEE ALSO

zmq\_tcp(7)
zmq\_curve(7)

Void Linux - February 22, 2018
