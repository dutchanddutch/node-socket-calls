# node-socket-calls
Simple nodejs wrapper (using N-API) for low-level socket calls.

Currently only supports Linux.

This module is work in progress.

The arguments and return value roughly follow the underlying system calls.
On error, the return value (or the first return value, if it has multiple) is a
negated error code instead of -1.

All file descriptors returned will have their close-on-exec flag set.  All
sockets created will be non-blocking.

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
