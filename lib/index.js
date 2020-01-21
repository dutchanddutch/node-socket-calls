'use strict';

const binding = require('bindings')('socket-calls.node');


const { sendto } = binding;
const send = ( fd, data, cmsgs, flags ) => sendto( fd, undefined, data, cmsgs, flags );


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


const sa_family = ( sa ) => sa.readUInt16LE( 0 );

const { AF_UNIX } = binding;
const { fileURLToPath } = require('url');

const sa_unix = ( path, encoding='utf8' ) => {
	if( path instanceof URL )
		path = fileURLToPath( path );

	if( path instanceof Buffer ) {
		if( path.indexOf( 0 ) > 0 )
			throw new TypeError("Path cannot contain a NUL byte (unless abstract)");

		let sa = Buffer.alloc( 2 + path.length );
		sa.writeUInt16LE( AF_UNIX, 0 );
		path.copy( sa, 2 );
		return sa;

	} else if( typeof path === 'string' ) {
		if( path.includes( "\x00" ) )
			throw new TypeError("Path cannot contain a NUL byte (unless abstract)");

		let sa = Buffer.alloc( 2 + Buffer.byteLength( path, encoding ) );
		sa.writeUInt16LE( AF_UNIX, 0 );
		sa.write( path, 2, encoding );
		return sa;

	} else if( path === undefined ) {
		let sa = Buffer.alloc( 2 );
		sa.writeUInt16LE( AF_UNIX, 0 );
		return sa;

	} else {
		throw new TypeError("Path argument must be string, buffer, URL object, or undefined");
	}
};

const sa_unix_path = ( sa, encoding='utf8' ) => {
	if( !( sa instanceof Buffer ) )
		throw new TypeError("Socket address must be a buffer");
	if( sa_family( sa ) !== AF_UNIX )
		throw new Error("Socket address family is not AF_UNIX");

	if( sa.length === 2 )
		return undefined;  // autobind

	let i = sa.indexOf( 0, 2 );
	if( i === 2 )
		return sa.slice( 2 );  // abstract namespace
	if( i < 0 )
		i = undefined;  // no NUL-terminator
	if( encoding === null )
		return sa.slice( 2, i );
	else
		return sa.toString( encoding, 2, i );
};


module.exports = {
	...binding,
	send,
	CMSGHDR_LEN,
	cmsg,
	cmsg_rights,
	cmsg_creds,
	sa_family,
	sa_unix, sa_unix_path
};

delete module.exports.path;
