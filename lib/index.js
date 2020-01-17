'use strict';

const binding = require('bindings')('socket-calls.node');


const send = ( fd, data, cmsgs, flags ) => binding.sendto( fd, undefined, data, cmsgs, flags );


const { CMSG_ALIGN, SCM_RIGHTS, SCM_CREDENTIALS, SOL_SOCKET } = binding;
const CMSGHDR_LEN = CMSG_ALIGN + 8;

const cmsg = function( level, type, datalen ) {
	const len = CMSGHDR_LEN + datalen;
	const msg = Buffer.alloc( len + (-len & (CMSG_ALIGN-1)) );
	msg.writeInt32LE( len, 0 );
	msg.writeInt32LE( level, CMSG_ALIGN );
	msg.writeInt32LE( type, CMSG_ALIGN + 4 );
	return msg;
};

const cmsg_rights = function( fds ) {
	let msg = cmsg( SOL_SOCKET, SCM_RIGHTS, 4 * fds.length );
	let pos = CMSGHDR_LEN;
	for( let fd of fds )
		pos = msg.writeInt32LE( fd, pos );
	return msg;
};

const cmsg_creds = function( pid, uid, gid ) {
	let msg = cmsg( SOL_SOCKET, SCM_CREDENTIALS, 12 );
	let pos = CMSGHDR_LEN;
	pos = msg.writeInt32LE( pid, pos );
	pos = msg.writeInt32LE( uid, pos );
	pos = msg.writeInt32LE( gid, pos );
	return msg;
};


const { AF_UNIX } = binding;

const sa_unix = ( path ) => {
	if( path instanceof Buffer ) {
		let sa = Buffer.alloc( 2 + path.Length );
		sa.writeUInt16LE( AF_UNIX, 0 );
		path.copy( sa, 2 );
		return sa;
	} else {
		if( typeof path !== 'string' )
			throw new TypeError("Path argument must be buffer or string");
		
		let sa = Buffer.alloc( 2 + Buffer.byteLength( path ) + 1 );
		sa.writeUInt16LE( AF_UNIX, 0 );
		sa.write( path, 2 );
		return sa;
	}
};

const parse_sa_unix = ( sa ) => {
	if( sa.readUInt16LE( 0 ) !== AF_UNIX )
		throw new Error("Not an AF_UNIX socket address");

	let i = sa.indexOf( 0 );
	if( i > 0 )
		return sa.slice( 2, i );  // NUL-terminated path
	else
		return sa.slice( 2 );  // unterminated path (i < 0) or abstract namespace (i == 0)
};


module.exports = {
	...binding,
	send,
	cmsg,
	cmsg_rights,
	cmsg_creds,
	sa_unix, parse_sa_unix
};

delete module.exports.path;
