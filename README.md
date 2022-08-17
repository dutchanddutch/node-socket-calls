# node-socket-calls
Simple nodejs wrapper (using N-API) for low-level socket calls.

Currently only supports Linux.

This module is work in progress.

The arguments and return value roughly follow the underlying system calls.
On error, the return value (or the first return value, if it has multiple) is a
negated error code instead of -1.

All file descriptors returned will have their close-on-exec flag set.  All
sockets created will be non-blocking.

## Table of contents

<!-- toc-begin -->
* [API summary](#api-summary)
* [Socket address utilities](#socket-address-utilities)
  * [`sa_family`( _addr_ )](#sa_family-addr-)
  * [`sa_unix`( [_path_[, _encoding_]] )](#sa_unix-path-encoding-)
  * [`sa_unix_path`( _addr_[, _encoding_] )](#sa_unix_path-addr-encoding-)
* [Exported constants](#exported-constants)
<!-- toc-end -->

## API summary

On failure these functions return
* `-errno` if they normally return an integer or boolean
* `[ -errno ]` if they normally return an array

```js
err             = close( fd );
newfd           = dup( fd, minfd=3 );           // uses fcntl F_DUPFD_CLOEXEC

err             = setcloexec( fd, bool=true );  // uses ioctl FIOCLEX/FIONCLEX
bool            = getcloexec( fd );             // uses fcntl F_GETFD

err             = setnonblocking( fd, bool=true );  // uses ioctl FIONBIO
bool            = getnonblocking( fd );             // uses fcntl F_GETFL

bool            = issocket( fd );               // uses isfdtype

nbytes          = getinq( fd );         // bytes in input buffer (SIOCINQ)
nbytes          = getoutq( fd );        // bytes in output buffer, unsent or unacknowledged (SIOCOUTQ)
nbytes          = getoutqnsd( fd );     // bytes in output buffer, unsent (SIOCOUTQNSD)

fd              = socket( domain, type, protocol );    
[ fd1, fd2 ]    = socketpair( domain, type, protocol );

res             = setsockopt( fd, level, optname, data );
res             = setsockopt( fd, level, optname, int );
res             = setsockopt( fd, level, optname, bool );

[ res, datalen ]= getsockopt( fd, level, optname, databuf );
int             = getsockopt_int( fd, level, optname );
bool            = getsockopt_bool( fd, level, optname );

err             = bind( fd, addr );
err             = connect( fd, addr );

addrlen         = getsockname( fd, addrbuf );
addrlen         = getpeername( fd, addrbuf );

err             = listen( fd, int );

err             = shutdown( fd, how );

bool            = sockatmark( fd );

// both of these are actually wrappers for accept4()
// flags will always implicitly include SOCK_CLOEXEC and SOCK_NONBLOCK
fd              = accept( fd, flags=0 );
[ fd, addrlen ] = acceptfrom( fd, addrbuf, flags=0 );

// all of these are actually wrappers for sendmsg()
// data may also be an array of buffers
// flags will always implicitly include MSG_DONTWAIT and MSG_NOSIGNAL
datalen         = send( fd, data, flags=0 );
datalen         = sendto( fd, data, addr, flags=0 );
datalen         = sendmsg( fd, data, addr, cmsgs, flags=0 );

// all of these are actually wrappers for recvmsg()
// databuf may also be an array of buffers
// flags will always implicitly include MSG_DONTWAIT and MSG_CMSG_CLOEXEC
// rflags is msg.msg_flags after the recvmsg() call
[ datalen, rflags ]                    = recv( fd, databuf, flags=0 );
[ datalen, addrlen, rflags ]           = recvfrom( fd, databuf, addrbuf, flags=0 );
[ datalen, addrlen, cmsgslen, rflags ] = recvmsg( fd, databuf, addrbuf, cmsgsbuf, flags=0 );
```

## Socket address utilities

The socket calls use raw socket addresses in buffers, but utilities are
provided to encode and decode these:

### `sa_family`( _addr_ )

* _addr_: Socket address (buffer).
* **returns**:  Address family (integer).

Extracts the address family (e.g. `AF_UNIX`) of a socket address.

### `sa_unix`( [_path_[, _encoding_]] )

* _path_: Socket path (string, URI object, buffer, or undefined). Defaults to `undefined`.
* _encoding_: Used to encode the path if string or URI object, otherwise irrelevant. Defaults to `'utf8'`.
* **returns**: Socket address (buffer).

Encodes a socket address of family `AF_UNIX`.

The path can be a filesystem path (string, URI object, or buffer) or an abstract namespace path (buffer whose
first byte is 0), and in either case the maximum length of the path (in bytes) is given by the constant
`UNIX_PATH_MAX`.  If the path `undefined` or has zero length, the autobind address is returned, which can be
passed to `bind()` to have the kernel pick a unique address in the abstract namespace.

### `sa_unix_path`( _addr_[, _encoding_] )

* _addr_: Socket address (buffer).
* _encoding_: Used to decode the path to a string provided _encoding_ is not `null` and the path is not in
  abstract namespace (in either case a buffer is returned).  Defaults to `'utf8'`.
* **returns**:  Socket path (string, buffer, or undefined).

Extracts the path of a socket address of family `AF_UNIX`.

The path can be a filesystem path (buffer if _encoding_ is `null`, string otherwise), an abstract namespace
path (buffer whose first byte is 0), or `undefined` if _addr_ is the autobind address.

## Exported constants

I made no attempt at completeness.  If one you need is missing, just create an
issue or send a pull request.

```c
AF_UNSPEC
AF_NETLINK
AF_UNIX
AF_INET
AF_INET6
AF_PACKET

SOCK_STREAM
SOCK_DGRAM
SOCK_SEQPACKET

UNIX_PATH_MAX

SHUT_RD
SHUT_WR
SHUT_RDWR

MSG_CONFIRM
MSG_DONTROUTE
MSG_EOR
MSG_ERRQUEUE
MSG_MORE
MSG_OOB
MSG_PEEK
MSG_TRUNC

SOL_SOCKET

SO_PASSCRED
SO_PEERCRED
SO_PASSSEC
SO_PEERSEC
SO_RCVBUF
SO_SNDBUF

SCM_RIGHTS
SCM_CREDENTIALS
SCM_SECURITY

EACCES
EADDRINUSE
EADDRNOTAVAIL
EAFNOSUPPORT
EAGAIN
EALREADY
EBADF
ECONNABORTED
ECONNREFUSED
ECONNRESET
EDESTADDRREQ
EHOSTDOWN
EHOSTUNREACH
EINPROGRESS
EINTR
EINVAL
EISCONN
ELOOP
EMFILE
EMSGSIZE
ENAMETOOLONG
ENETDOWN
ENETUNREACH
ENFILE
ENOBUFS
ENOENT
ENOMEM
ENOPROTOOPT
ENOTCONN
ENOTDIR
ENOTSOCK
EOPNOTSUPP
EPERM
EPFNOSUPPORT
EPIPE
EPROTONOSUPPORT
EPROTOTYPE
ERANGE
EROFS
ESHUTDOWN
ESOCKTNOSUPPORT
ETOOMANYREFS
```
<!-- vim: tw=111 sts=8 expandtab smarttab
-->
