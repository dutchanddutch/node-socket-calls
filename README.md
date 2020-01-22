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
* [Exported constants](#exported-constants)
<!-- toc-end -->

## API summary

```js
err             = close( fd );

err             = setcloexec( fd, bool=true );  // uses ioctl FIOCLEX/FIONCLEX
bool            = getcloexec( fd );             // uses fcntl F_GETFD

bool		= issocket( fd );		// uses isfdtype

nbytes		= getinq( fd );		// bytes in input buffer (SIOCINQ)
nbytes		= getoutq( fd );	// bytes in output buffer, unsent or unacknowledged (SIOCOUTQ)
nbytes		= getoutqnsd( fd );	// bytes in output buffer, unsent (SIOCOUTQNSD)

fd              = socket( domain, type, protocol );    
[ fd1, fd2 ]    = socketpair( domain, type, protocol );

res             = setsockopt( fd, level, optname, data );
res             = setsockopt( fd, level, optname, int );
res             = setsockopt( fd, level, optname, bool );

[ res, datalen ]= getsockopt( fd, level, optname, databuf );
int    		= getsockopt_int( fd, level, optname );
bool   		= getsockopt_bool( fd, level, optname );

err             = bind( fd, addr );
err             = connect( fd, addr );

addrlen         = getsockname( fd, addrbuf );
addrlen         = getpeername( fd, addrbuf );

err             = listen( fd, int );

fd              = accept( fd );
[ fd, addrlen ] = acceptfrom( fd, addrbuf );

err		= shutdown( fd, how );

bool		= sockatmark( fd );

// API for send/sendto is still subject to change!
datalen		= send( fd, data, controldata=undefined, flags=0 );
datalen		= sendto( fd, addr, data, controldata=undefined, flags=0 );

// recv/recvfrom is not yet implemented
```

## Socket address utilities

```js
// address construction

addr		= sa_unix( path, encoding='utf8' );

// address parsing

family		= sa_family( addr );

path		= sa_unix_path( addr, encoding='utf8' );
path		= sa_unix_path( addr, null );  // always returns Buffer
```

The path of a unix (`AF_UNIX`) socket may be a normal (absolute or relative)
filesystem path, or it may be an identifier in the "abstrace namespace", which
are represented as buffers those first byte is 0.


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
SO_RCVBUF
SO_SNDBUF

SCM_RIGHTS
SCM_CREDENTIALS

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
