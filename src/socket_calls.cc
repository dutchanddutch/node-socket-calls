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
#include <linux/un.h>
#include <linux/sockios.h>

#ifndef SCM_SECURITY
#define SCM_SECURITY 0x03
#endif

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

template< typename Ret >
let result_bool( Env env, Ret ret )
{
	return result( env, ret, (bool)ret );
}

template< typename ...Val >
let create_array( Env env, Val const &...val )
{
	let array = Array::New( env, sizeof...(Val) );
	let i = 0u;
	( ..., array.Set( i++, val ) );
	return array;
}

template< typename Ret, typename ...Val >
let result2( Env env, Ret ret, Val const &...val )
{
	if( ret < 0 )
		return create_array( env, -errno );
	return create_array( env, val... );
}

template< typename Ret, typename ...Val >
let result3( Env env, Ret ret, Val const &...val )
{
	return result2( env, ret, ret, val... );
}

let int_arg( Value arg ) -> int
{
	return arg.As<Number>().Int32Value();
}

let int_arg( Value arg, int def_val ) -> int
{
	return arg.IsUndefined() ? def_val : int_arg( arg );
}

let buffer_arg( Value const &arg, void *&ptr_out ) -> size_t
{
	if( arg.IsUndefined() ) {
		ptr_out = nullptr;
		return 0;
	}
	let buf = arg.As<Buffer<u8>>();
	ptr_out = buf.Data();
	return buf.Length();
}

let js_getcloexec( CallbackInfo const &args )
{
	let env = args.Env();
	let fd = int_arg( args[0] );
	return result_bool( env, getcloexec( fd ) );
}

let js_setcloexec( CallbackInfo const &args )
{
	let env = args.Env();
	let fd = int_arg( args[0] );
	let cloexec = args[1].IsUndefined() || args[1].ToBoolean().Value();
	return result( env, setcloexec( fd, cloexec ) );
}

let js_getnonblocking( CallbackInfo const &args )
{
	let env = args.Env();
	let fd = int_arg( args[0] );
	return result_bool( env, getnonblocking( fd ) );
}

let js_setnonblocking( CallbackInfo const &args )
{
	let env = args.Env();
	let fd = int_arg( args[0] );
	let nonblocking = args[1].IsUndefined() || args[1].ToBoolean().Value();
	return result( env, setnonblocking( fd, nonblocking ) );
}

let js_close( CallbackInfo const &args )
{
	let env = args.Env();
	let fd = int_arg( args[0] );

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

let js_dup( CallbackInfo const &args )
{
	let env = args.Env();
	let fd = int_arg( args[0] );
	let minfd = int_arg( args[1], 3 );
	return result( env, fcntl( fd, F_DUPFD_CLOEXEC, minfd ) );
}


let js_socket( CallbackInfo const &args )
{
	let env = args.Env();
	let domain   = int_arg( args[0] );
	let type     = int_arg( args[1] ) | SOCK_CLOEXEC | SOCK_NONBLOCK;
	let protocol = int_arg( args[2], 0 );
	return result( env, socket( domain, type, protocol ) );
}

let js_socketpair( CallbackInfo const &args )
{
	let env = args.Env();
	let domain   = int_arg( args[0] );
	let type     = int_arg( args[1] ) | SOCK_CLOEXEC | SOCK_NONBLOCK;
	let protocol = int_arg( args[2], 0 );
	int fds[2];
	return result2( env, socketpair( domain, type, protocol, fds ), fds[0], fds[1] );
}

let js_getsockopt( CallbackInfo const &args )
{
	let env = args.Env();
	let fd      = int_arg( args[0] );
	let level   = int_arg( args[1] );
	let optname = int_arg( args[2] );
	let buf     = args[3].As<Buffer<u8>>();
	let len = (socklen_t) buf.Length();
	return result3( env, getsockopt( fd, level, optname, buf.Data(), &len ), len );
}

let js_getsockopt_int( CallbackInfo const &args )
{
	let env = args.Env();
	let fd      = int_arg( args[0] );
	let level   = int_arg( args[1] );
	let optname = int_arg( args[2] );
	let val = int{};
	let len = socklen_t{ sizeof val };
	return result( env, getsockopt( fd, level, optname, &val, &len ), val );
}

let js_getsockopt_bool( CallbackInfo const &args )
{
	let env = args.Env();
	let fd      = int_arg( args[0] );
	let level   = int_arg( args[1] );
	let optname = int_arg( args[2] );
	let val = int{};
	let len = socklen_t{ sizeof val };
	return result( env, getsockopt( fd, level, optname, &val, &len ), val != 0 );
}

let js_setsockopt( CallbackInfo const &args )
{
	let env = args.Env();
	let fd      = int_arg( args[0] );
	let level   = int_arg( args[1] );
	let optname = int_arg( args[2] );
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
	let fd = int_arg( args[0] );
	let addr = args[1].As<Buffer<u8>>();
	return result( env, bind( fd, (sockaddr const *)addr.Data(), addr.Length() ) );
}

let js_connect( CallbackInfo const &args )
{
	let env = args.Env();
	let fd = int_arg( args[0] );
	let addr = args[1].As<Buffer<u8>>();
	return result( env, connect( fd, (sockaddr const *)addr.Data(), addr.Length() ) );
}

let js_getsockname( CallbackInfo const &args )
{
	let env = args.Env();
	let fd = int_arg( args[0] );
	let buf = args[1].As<Buffer<u8>>();
	let len = (socklen_t) buf.Length();
	return result( env, getsockname( fd, (sockaddr *)buf.Data(), &len ), len );
}

let js_getpeername( CallbackInfo const &args )
{
	let env = args.Env();
	let fd = int_arg( args[0] );
	let buf = args[1].As<Buffer<u8>>();
	let len = (socklen_t) buf.Length();
	return result( env, getpeername( fd, (sockaddr *)buf.Data(), &len ), len );
}

let js_listen( CallbackInfo const &args )
{
	let env = args.Env();
	let fd = int_arg( args[0] );
	let n = int_arg( args[1] );
	return result( env, listen( fd, n ) );
}

let js_accept( CallbackInfo const &args )
{
	let env = args.Env();
	let fd = int_arg( args[0] );
	let flags = int_arg( args[1], 0 ) | SOCK_CLOEXEC | SOCK_NONBLOCK;
	return result( env, accept4( fd, nullptr, nullptr, flags ) );
}

let js_acceptfrom( CallbackInfo const &args )
{
	let env = args.Env();
	let fd = int_arg( args[0] );
	let buf = args[1].As<Buffer<u8>>();
	let len = (socklen_t) buf.Length();
	let flags = int_arg( args[2], 0 ) | SOCK_CLOEXEC | SOCK_NONBLOCK;
	return result3( env, accept4( fd, (sockaddr *)buf.Data(), &len, flags ), len );
}

let js_shutdown( CallbackInfo const &args )
{
	let env = args.Env();
	let fd = int_arg( args[0] );
	let how = int_arg( args[1] );
	return result( env, shutdown( fd, how ) );
}

let js_sockatmark( CallbackInfo const &args )
{
	let env = args.Env();
	let fd = int_arg( args[0] );
	return result_bool( env, sockatmark( fd ) );
}

template< typename T >
let js_ioctl_read( CallbackInfo const &args, unsigned long request )
{
	let env = args.Env();
	let fd = int_arg( args[0] );
	let value = T{};
	return result( env, ioctl( fd, request, &value ), value );
}

let js_getinq( CallbackInfo const &args ) {  return js_ioctl_read<int>( args, SIOCINQ );  }
let js_getoutq( CallbackInfo const &args ) {  return js_ioctl_read<int>( args, SIOCOUTQ );  }
let js_getoutqnsd( CallbackInfo const &args ) {  return js_ioctl_read<int>( args, SIOCOUTQNSD );  }

let js_issocket( CallbackInfo const &args )
{
	let env = args.Env();
	let fd = int_arg( args[0] );
	return result_bool( env, isfdtype( fd, S_IFSOCK ) );
}

let inline js_send_recv_common( CallbackInfo const &args, int nargs, bool send ) -> Value
{
	let env = args.Env();

	let fd = int_arg( args[0] );
	let flags = int_arg( args[ nargs - 1 ], 0 );

	let iovlen = 1u;
	if( args[1].IsArray() )
		iovlen = args[1].As<Array>().Length();
	iovec iov[ iovlen ];

	if( args[1].IsArray() ) {
		let bufs = args[1].As<Array>();
		for( let i = 0u; i < iovlen; ++i )
			iov[i].iov_len = buffer_arg( bufs[i], iov[i].iov_base );
	} else {
		iov[0].iov_len = buffer_arg( args[1], iov[0].iov_base );
	}

	let msg = msghdr{};
	msg.msg_iov = iov;
	msg.msg_iovlen = iovlen;

	if( nargs >= 4 )
		msg.msg_namelen = (socklen_t)buffer_arg( args[2], msg.msg_name );
	if( nargs >= 5 )
		msg.msg_controllen = (socklen_t)buffer_arg( args[3], msg.msg_control );

	if( send )
		return result( env, sendmsg( fd, &msg, flags | MSG_DONTWAIT | MSG_NOSIGNAL ) );

	let datalen = recvmsg( fd, &msg, flags | MSG_DONTWAIT | MSG_CMSG_CLOEXEC );
	let rflags = msg.msg_flags & ~MSG_CMSG_CLOEXEC;

	if( nargs <= 3 )
		return result3( env, datalen, rflags );
	else if( nargs == 4 )
		return result3( env, datalen, msg.msg_namelen, rflags );
	else
		return result3( env, datalen, msg.msg_namelen, msg.msg_controllen, rflags );
}

template< int nargs >
let js_send( CallbackInfo const &args )
{
	static_assert( nargs >= 3 && nargs <= 5 );
	return js_send_recv_common( args, nargs, true );
}

template< int nargs >
let js_recv( CallbackInfo const &args )
{
	static_assert( nargs >= 3 && nargs <= 5 );
	return js_send_recv_common( args, nargs, false );
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

	set_constant( exports, UNIX_PATH_MAX );

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
	set_constant( exports, SO_PASSSEC );
	set_constant( exports, SO_PEERSEC );
	set_constant( exports, SO_RCVBUF );
	set_constant( exports, SO_SNDBUF );

	set_constant( exports, SCM_RIGHTS );
	set_constant( exports, SCM_CREDENTIALS );
	set_constant( exports, SCM_SECURITY );

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
	set_function( exports, "getnonblocking",js_getnonblocking );
	set_function( exports, "setnonblocking",js_setnonblocking );
	set_function( exports, "close",		js_close );
	set_function( exports, "dup",		js_dup );

	set_function( exports, "socket",	js_socket );
	set_function( exports, "socketpair",	js_socketpair );
	set_function( exports, "getsockopt",	js_getsockopt );
	set_function( exports, "getsockopt_int",js_getsockopt_int );
	set_function( exports, "getsockopt_bool",js_getsockopt_bool );
	set_function( exports, "setsockopt",	js_setsockopt );
	set_function( exports, "bind",		js_bind );
	set_function( exports, "connect",	js_connect );
	set_function( exports, "getsockname",	js_getsockname );
	set_function( exports, "getpeername",	js_getpeername );
	set_function( exports, "listen",	js_listen );
	set_function( exports, "accept",	js_accept );
	set_function( exports, "acceptfrom",	js_acceptfrom );
	set_function( exports, "shutdown",	js_shutdown );
	set_function( exports, "send",		js_send<3> );
	set_function( exports, "sendto",	js_send<4> );
	set_function( exports, "sendmsg",	js_send<5> );
	set_function( exports, "recv",		js_recv<3> );
	set_function( exports, "recvfrom",	js_recv<4> );
	set_function( exports, "recvmsg",	js_recv<5> );
	set_function( exports, "sockatmark",	js_sockatmark );
	set_function( exports, "getoutqnsd",	js_getoutqnsd );
	set_function( exports, "getoutq",	js_getoutq );
	set_function( exports, "getinq",	js_getinq );
	set_function( exports, "issocket",	js_issocket );

	return exports;
}

} // anonymous namespace

NODE_API_MODULE( socket_calls, initialize )
