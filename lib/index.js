'use strict';

const binding = require('bindings')('socket-calls.node');


// utilities for reading/writing control messages (API not stabilized)

const { CMSG_ALIGN } = binding;
const CMSG_ALIGN_MASK = CMSG_ALIGN - 1;
const CMSGHDR_LEN = CMSG_ALIGN + 4 + 4;
const cmsg_align = ( len ) => len + ( -len & CMSG_ALIGN_MASK );
const cmsg_space = ( datalen ) => CMSGHDR_LEN + cmsg_align( datalen );

const cmsghdr_write = ( buf, level, type, datalen, offset = 0 ) => {
	if( !( buf instanceof Buffer ) )
		throw new TypeError(`Control message buffer must be a Buffer`);
	let len = CMSGHDR_LEN + datalen;
	buf.writeUInt32LE( len, offset );  offset += CMSG_ALIGN;
	offset = buf.writeInt32LE( level, offset );
	offset = buf.writeInt32LE( type, offset );
	return offset;
};

const cmsg_write = ( buf, level, type, data, offset = 0 ) => {
	if( !( data instanceof Buffer ) )
		throw new TypeError(`Control message data must be a Buffer`);
	if( offset & CMSG_ALIGN_MASK )
		throw new RangeError('Control message misaligned in buffer');
	offset = cmsghdr_write( buf, level, type, data.length, offset );
	data.copy( buf, offset );
	offset = cmsg_align( offset + data.length );
	if( offset > buf.length )
		throw new RangeError(`Control message does not fit in buffer`);
	return offset;
};

const cmsgs_join = ( ...cmsgs ) => {
	let len = 0;
	for( let { data } of cmsgs )
		len += cmsg_space( data.length );
	let buf = Buffer.alloc( len );
	let offset = 0;
	for( let { level, type, data } of cmsgs )
		offset = cmsg_write( buf, level, type, data, offset );
	return buf;
};

const cmsghdr_read = ( buf, offset = 0 ) => {
	if( !( buf instanceof Buffer ) )
		throw new TypeError(`Control data buffer must be a Buffer`);
	let len   = buf.readUInt32LE( offset );
	let level = buf.readUInt32LE( offset + CMSG_ALIGN );
	let type  = buf.readUInt32LE( offset + CMSG_ALIGN + 4 );
	let datalen = len - CMSGHDR_LEN;
	if( CMSG_ALIGN >= 8 && buf.readUInt32LE( offset + 4 ) )
		throw new Error("Invalid control message (length >= 2**32)");
	if( datalen < 0 )
		throw new Error("Invalid control message (length < header length)");
	return { level, type, datalen };
};

const cmsg_read = ( buf, offset = 0 ) => {
	if( offset & CMSG_ALIGN_MASK )
		throw new RangeError('Control message misaligned in buffer');
	let { level, type, datalen } = cmsghdr_read( buf, offset );
	offset += CMSGHDR_LEN;
	let end = offset + datalen;
	if( end > buf.length )
		throw new Error("Control message extends beyond end of control data buffer");
	let data = buf.subarray( offset, end );
	return { level, type, data };
};

const cmsgs_split = ( buf, len = buf.length ) => {
	let cmsgs = [];
	if( len === 0 )
		return cmsgs;
	if( !( buf instanceof Buffer ) )
		throw new TypeError(`Control data buffer must be a Buffer`);
	if( len !== ( len >>> 0 ) || len > buf.length )
		throw new Error("Invalid control data length");
	let offset = 0;
	while( offset + CMSGHDR_LEN <= len ) {
		let msg = cmsg_read( buf, offset );
		cmsgs.push( msg );
		offset += cmsg_space( msg.data.length );
		if( offset > len )
			throw new Error("Control message extends beyond end of control data buffer");
	}
	return cmsgs;
};


// socket address utilities

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
		i = sa.length;  // no NUL-terminator
	if( encoding === null )
		return sa.slice( 2, i );
	else
		return sa.toString( encoding, 2, i );
};


delete binding.path;

module.exports = {
	...binding,
	CMSG_ALIGN_MASK, CMSGHDR_LEN, cmsg_align, cmsg_space,
	cmsghdr_write, cmsg_write, cmsgs_join,
	cmsghdr_read, cmsg_read, cmsgs_split,
	sa_family,
	sa_unix, sa_unix_path,
};
