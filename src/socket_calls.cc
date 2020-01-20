#ifndef __linux__
# error "Only Linux is supported"
#endif

#define NAPI_CPP_EXCEPTIONS 1
#include <napi.h>

#include <type_traits>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <linux/sockios.h>

#define let auto

using u8 = uint8_t;


namespace {

// File descriptor flags.
//
// There's currently only one fd-flag: FD_CLOEXEC

let inline getfdflags( int fd) -> int {  return fcntl(fd, F_GETFD );  }
let inline setfdflags( int fd, int flags) -> int {  return fcntl(fd, F_SETFD, flags );  }

let inline getcloexec( int fd ) -> int {
	let flags = getfdflags( fd );
	return flags < 0 ? flags : ( flags & FD_CLOEXEC ) != 0;
}

let inline setcloexec( int fd, bool cloexec = true ) -> int {
	return ioctl( fd, cloexec ? FIOCLEX : FIONCLEX );
}

// Open-flags.
//
// There are lots of open-flags, but only a few can be modified.

let inline getoflags( int fd) -> int {  return fcntl(fd, F_GETFL );  }
let inline setoflags( int fd, int flags) -> int {  return fcntl(fd, F_SETFL, flags );  }

let inline getnonblocking( int fd ) -> int {
	let flags = getoflags( fd );
	return flags < 0 ? flags : ( flags & O_NONBLOCK ) != 0;
}

let inline setnonblocking( int fd, bool nonblocking = true ) -> int {
	let arg = int{ nonblocking };
	return ioctl( fd, FIONBIO, &arg );
}



using namespace Napi;

template< typename Ret >
let result( Env env, Ret ret )
{
	return Value::From( env, ret < 0 ? -errno : ret );
}

template< typename Ret, typename Val >
let result( Env env, Ret ret, Val const &val )
{
	if( ret < 0 )
		return Value::From( env, -errno );
	return Value::From( env, val );
}

template< typename Ret, typename Val1, typename Val2 >
let result2( Env env, Ret ret, Val1 const &val1, Val2 const &val2 )
{
	let result = Array::New( env, 2 );
	if( ret < 0 ) {
		result[0u] = -errno;
		result[1u] = env.Undefined();
	} else {
		result[0u] = val1;
		result[1u] = val2;
	}
	return result;
}

template< typename Ret, typename Val2 >
let result2( Env env, Ret ret, Val2 const &val2 )
{
	return result2( env, ret, ret, val2 );
}

let js_getcloexec( CallbackInfo const &args )
{
	let env = args.Env();
	let fd = args[0].As<Number>().Int32Value();
	let flags = getfdflags( fd );
	return result( env, flags, !!( flags & FD_CLOEXEC ) );
}

let js_setcloexec( CallbackInfo const &args )
{
	let env = args.Env();
	let fd = args[0].As<Number>().Int32Value();
	let cloexec = args[1].IsUndefined() || args[1].ToBoolean().Value();
	return result( env, setcloexec( fd, cloexec ) );
}

let js_close( CallbackInfo const &args )
{
	let env = args.Env();
	let fd = args[0].As<Number>().Int32Value();

	// POSIX 2008 states that it unspecified what the state of a file descriptor is if
	// close() is interrupted by a signal and fails with EINTR.  This is a problem for
	// multi-threaded programs since if the fd was actually closed it may already be reused
	// by another thread hence it is unsafe to attempt to close it again.
	//
	// In 2012, POSIX approved a clarification that aimed to deal with this mess:
	//	http://austingroupbugs.net/view.php?id=529#c1200
	//
	// The short summary is that if the fd is never valid after close(), as is the case on
	// Linux, then <unistd.h> should add:
	//
	//      #define POSIX_CLOSE_RESTART 0
	//
	// and the new posix_close() should be implemented as something like:
	//
	//      int posix_close( int fd, int flags ) {
	//          int r = close( fd );
	//          if ( r < 0 && errno == EINTR )
	//              return 0 or set errno to EINPROGRESS;
	//          return r;
	//      }
	//
	// In contrast, on systems where EINTR means the close() didn't happen (like HP-UX),
	// POSIX_CLOSE_RESTART should be non-zero and if passed as flag to posix_close() it
	// should automatically retry close() on EINTR.
	//
	// Of course this is nice and all, but apparently adding one constant and a trivial
	// wrapper was way too much effort for the glibc project:
	//      https://sourceware.org/bugzilla/show_bug.cgi?id=16302
	//
	// But since we actually only care about Linux, we don't have to worry about all this
	// anyway.  close() always means the fd is gone, even if an error occurred.  This
	// elevates EINTR to the status of real error, since it implies behaviour associated
	// with close ( e.g. flush ) was aborted and can not be retried since the fd is gone.

	return result( env, close( fd ) );
}

let js_socket( CallbackInfo const &args )
{
	let env = args.Env();
	let domain   = args[0].As<Number>().Int32Value();
	let type     = args[1].As<Number>().Int32Value() | SOCK_NONBLOCK | SOCK_CLOEXEC;
	let protocol = args.Length() >= 3 ? args[2].As<Number>().Int32Value() : 0;
	return result( env, socket( domain, type, protocol ) );
}

let js_socketpair( CallbackInfo const &args )
{
	let env = args.Env();
	let domain   = args[0].As<Number>().Int32Value();
	let type     = args[1].As<Number>().Int32Value() | SOCK_NONBLOCK | SOCK_CLOEXEC;
	let protocol = args.Length() >= 3 ? args[2].As<Number>().Int32Value() : 0;
	int fds[2];
	return result2( env, socketpair( domain, type, protocol, fds ), fds[0], fds[1] );
}

let js_getsockopt( CallbackInfo const &args )
{
	let env = args.Env();
	let fd      = args[0].As<Number>().Int32Value();
	let level   = args[1].As<Number>().Int32Value();
	let optname = args[2].As<Number>().Int32Value();
	if( args[3].IsUndefined() ) {
		let val = int{};
		let len = socklen_t{ sizeof val };
		return result2( env, getsockopt( fd, level, optname, &val, &len ), val );
	} else {
		let buf = args[3].As<Buffer<u8>>();
		let len = (socklen_t) buf.Length();
		return result2( env, getsockopt( fd, level, optname, buf.Data(), &len ), len );
	}
}

let js_setsockopt( CallbackInfo const &args )
{
	let env = args.Env();
	let fd      = args[0].As<Number>().Int32Value();
	let level   = args[1].As<Number>().Int32Value();
	let optname = args[2].As<Number>().Int32Value();
	if( args[3].IsNumber() || args[3].IsBoolean() ) {
		let val = args[3].ToNumber().Int32Value();
		return result( env, setsockopt( fd, level, optname, &val, sizeof val ) );
	} else {
		let buf = args[3].As<Buffer<u8>>();
		return result( env, setsockopt( fd, level, optname, buf.Data(), buf.Length() ) );
	}
}

let js_bind( CallbackInfo const &args )
{
	let env = args.Env();
	let fd = args[0].As<Number>().Int32Value();
	let addr = args[1].As<Buffer<u8>>();
	return result( env, bind( fd, (sockaddr const *)addr.Data(), addr.Length() ) );
}

let js_connect( CallbackInfo const &args )
{
	let env = args.Env();
	let fd = args[0].As<Number>().Int32Value();
	let addr = args[1].As<Buffer<u8>>();
	return result( env, connect( fd, (sockaddr const *)addr.Data(), addr.Length() ) );
}

let js_getsockname( CallbackInfo const &args )
{
	let env = args.Env();
	let fd = args[0].As<Number>().Int32Value();
	let buf = args[1].As<Buffer<u8>>();
	let len = (socklen_t) buf.Length();
	return result( env, getsockname( fd, (sockaddr *)buf.Data(), &len ), len );
}

let js_getpeername( CallbackInfo const &args )
{
	let env = args.Env();
	let fd = args[0].As<Number>().Int32Value();
	let buf = args[1].As<Buffer<u8>>();
	let len = (socklen_t) buf.Length();
	return result( env, getpeername( fd, (sockaddr *)buf.Data(), &len ), len );
}

let js_listen( CallbackInfo const &args )
{
	let env = args.Env();
	let fd = args[0].As<Number>().Int32Value();
	let n = args[1].As<Number>().Int32Value();
	return result( env, listen( fd, n ) );
}

let js_accept( CallbackInfo const &args )
{
	let env = args.Env();
	let fd = args[0].As<Number>().Int32Value();
	let flags = args.Length() >= 2 ? args[1].As<Number>().Int32Value() : 0;
	return result( env, accept4( fd, nullptr, nullptr, flags | SOCK_CLOEXEC | SOCK_NONBLOCK ) );
}

let js_acceptfrom( CallbackInfo const &args )
{
	let env = args.Env();
	let fd = args[0].As<Number>().Int32Value();
	let buf = args[1].As<Buffer<u8>>();
	let len = (socklen_t) buf.Length();
	let flags = args.Length() >= 3 ? args[2].As<Number>().Int32Value() : 0;
	flags |= SOCK_CLOEXEC | SOCK_NONBLOCK;
	return result2( env, accept4( fd, (sockaddr *)buf.Data(), &len, flags ), len );
}

let js_shutdown( CallbackInfo const &args )
{
	let env = args.Env();
	let fd = args[0].As<Number>().Int32Value();
	let how = args[1].As<Number>().Int32Value();
	return result( env, shutdown( fd, how ) );
}

let js_sockatmark( CallbackInfo const &args )
{
	let env = args.Env();
	let fd = args[0].As<Number>().Int32Value();
	let res = sockatmark( fd );
	return result( env, res, res > 0 );
}

template< typename T >
let js_ioctl_read( CallbackInfo const &args, unsigned long request )
{
	let env = args.Env();
	let fd = args[0].As<Number>().Int32Value();
	let value = T{};
	return result( env, ioctl( fd, request, &value ), value );
}

let js_getinq( CallbackInfo const &args ) {  return js_ioctl_read<int>( args, SIOCINQ );  }
let js_getoutq( CallbackInfo const &args ) {  return js_ioctl_read<int>( args, SIOCOUTQ );  }
let js_getoutqnsd( CallbackInfo const &args ) {  return js_ioctl_read<int>( args, SIOCOUTQNSD );  }

let js_issocket( CallbackInfo const &args )
{
	let env = args.Env();
	let fd = args[0].As<Number>().Int32Value();
	return result( env, isfdtype( fd, S_IFSOCK ) );
}

let js_sendto( CallbackInfo const &args )
{
	let env = args.Env();
	let fd = args[0].As<Number>().Int32Value();
	let msg = msghdr{};
	let addr = args[1].As<Buffer<u8>>();
	if( ! addr.IsUndefined() ) {
		msg.msg_name = addr.Data();
		msg.msg_namelen = (socklen_t)addr.Length();
	}
	let buf = args[2].As<Buffer<u8>>();
	let iov = iovec{ buf.Data(), buf.Length() };
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	if( ! args[3].IsUndefined() ) {
		let cmsgs = args[3].As<Buffer<u8>>();
		msg.msg_control = cmsgs.Data();
		msg.msg_controllen = (socklen_t)cmsgs.Length();
	}
	let flags = args[4].IsUndefined() ? 0 : args[4].As<Number>().Int32Value();
	return result( env, sendmsg( fd, &msg, flags | MSG_DONTWAIT | MSG_NOSIGNAL ) );
}

template< typename T >
let inline set_property( Object obj, char const *name, T value )
{
	if constexpr ( std::is_enum_v<T> )
		obj[ name ] = (std::underlying_type_t<T>) value;
	else
		obj[ name ] = value;
}

#define set_constant( target, constant ) \
	set_property( (target), #constant, (constant) )

template< typename T >
let inline set_function( Object obj,  char const *name, T fun )
{
	set_property( obj, name, Function::New( obj.Env(), fun, name ) );
}

Object initialize( Env env, Object exports )
{
	set_constant( exports, AF_UNSPEC );
	set_constant( exports, AF_NETLINK );
	set_constant( exports, AF_UNIX );
	set_constant( exports, AF_INET );
	set_constant( exports, AF_INET6 );
	set_constant( exports, AF_PACKET );

	set_constant( exports, SOCK_STREAM );
	set_constant( exports, SOCK_DGRAM );
	set_constant( exports, SOCK_SEQPACKET );

	set_constant( exports, MSG_EOR );
	set_constant( exports, MSG_MORE );

	set_property( exports, "CMSG_ALIGN", (uint32_t) CMSG_ALIGN(1) );

	set_constant( exports, SHUT_RD );
	set_constant( exports, SHUT_WR );
	set_constant( exports, SHUT_RDWR );

	set_constant( exports, MSG_CONFIRM );
	set_constant( exports, MSG_DONTROUTE );
	set_constant( exports, MSG_EOR );
	set_constant( exports, MSG_ERRQUEUE );
	set_constant( exports, MSG_MORE );
	set_constant( exports, MSG_OOB );
	set_constant( exports, MSG_PEEK );
	set_constant( exports, MSG_TRUNC );

	set_constant( exports, SOL_SOCKET );

	set_constant( exports, SO_PASSCRED );
	set_constant( exports, SO_PEERCRED );
	set_constant( exports, SO_RCVBUF );
	set_constant( exports, SO_SNDBUF );

	set_constant( exports, SCM_RIGHTS );
	set_constant( exports, SCM_CREDENTIALS );

	set_constant( exports, EACCES );
	set_constant( exports, EADDRINUSE );	// Address already in use
	set_constant( exports, EADDRNOTAVAIL );	// Cannot assign requested address
	set_constant( exports, EAFNOSUPPORT );	// Address family not supported by protocol
	set_constant( exports, EAGAIN );
	set_constant( exports, EALREADY );	// Operation already in progress
	set_constant( exports, EBADF );
	set_constant( exports, ECONNABORTED );	// Software caused connection abort
	set_constant( exports, ECONNREFUSED );	// Connection refused
	set_constant( exports, ECONNRESET );	// Connection reset by peer
	set_constant( exports, EDESTADDRREQ );	// Destination address required
	set_constant( exports, EHOSTDOWN );	// Host is down
	set_constant( exports, EHOSTUNREACH );	// No route to host
	set_constant( exports, EINPROGRESS );	// Operation now in progress (not an error)
	set_constant( exports, EINTR );
	set_constant( exports, EINVAL );
	set_constant( exports, EISCONN );	// Transport endpoint is already connected
	set_constant( exports, ELOOP );
	set_constant( exports, EMFILE );
	set_constant( exports, EMSGSIZE );	// Message too long
	set_constant( exports, ENAMETOOLONG );
	set_constant( exports, ENETDOWN );	// Network is down
	set_constant( exports, ENETUNREACH );	// Network is unreachable
	set_constant( exports, ENFILE );
	set_constant( exports, ENOBUFS );	// No buffer space available
	set_constant( exports, ENOENT );
	set_constant( exports, ENOMEM );
	set_constant( exports, ENOPROTOOPT );	// Protocol not available
	set_constant( exports, ENOTCONN );	// Transport endpoint is not connected
	set_constant( exports, ENOTDIR );
	set_constant( exports, ENOTSOCK );	// Socket operation on non-socket
	set_constant( exports, EOPNOTSUPP );	// Operation not supported on transport endpoint
	set_constant( exports, EPERM );
	set_constant( exports, EPFNOSUPPORT );	// Protocol family not supported
	set_constant( exports, EPIPE );
	set_constant( exports, EPROTONOSUPPORT );// Protocol not supported
	set_constant( exports, EPROTOTYPE );	// Protocol wrong type for socket
	set_constant( exports, ERANGE );
	set_constant( exports, EROFS );
	set_constant( exports, ESHUTDOWN );	// Cannot send after shutdown
	set_constant( exports, ESOCKTNOSUPPORT );// Socket type not supported
	set_constant( exports, ETOOMANYREFS );	// Too many file descriptors in flight

	set_function( exports, "getcloexec",	js_getcloexec );
	set_function( exports, "setcloexec",	js_setcloexec );
	set_function( exports, "close",		js_close );
	set_function( exports, "socket",	js_socket );
	set_function( exports, "socketpair",	js_socketpair );
	set_function( exports, "getsockopt",	js_getsockopt );
	set_function( exports, "setsockopt",	js_setsockopt );
	set_function( exports, "bind",		js_bind );
	set_function( exports, "connect",	js_connect );
	set_function( exports, "getsockname",	js_getsockname );
	set_function( exports, "getpeername",	js_getpeername );
	set_function( exports, "listen",	js_listen );
	set_function( exports, "accept",	js_accept );
	set_function( exports, "acceptfrom",	js_acceptfrom );
	set_function( exports, "shutdown",	js_shutdown );
	set_function( exports, "sendto",	js_sendto );
//	set_function( exports, "recvfrom",	js_recvfrom );	// TODO
	set_function( exports, "sockatmark",	js_sockatmark );
	set_function( exports, "getoutqnsd",	js_getoutqnsd );
	set_function( exports, "getoutq",	js_getoutq );
	set_function( exports, "getinq",	js_getinq );
	set_function( exports, "issocket",	js_issocket );

	return exports;
}

} // anonymous namespace

NODE_API_MODULE( socket_calls, initialize )
