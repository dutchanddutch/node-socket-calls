'use strict';

const assert = require('assert');

const { AF_UNIX, sa_family, sa_unix, sa_unix_path } = require('..');

const mkbuf = ( ...args ) =>
	Buffer.concat( args.map( x => Buffer.from( typeof x === 'number' ? [x] : x ) ) );

const matchbuf = ( buf, ...args ) =>
	assert.deepStrictEqual( buf, mkbuf( ...args ) );

describe( "sa_unix", function() {
	it( "works for autobind address", function() {
		matchbuf( sa_unix(), AF_UNIX, 0 );
		matchbuf( sa_unix( undefined ), AF_UNIX, 0 );
	});
	it( "works for default (utf-8) encoded paths", function() {
		let path = "héllo";
		matchbuf( sa_unix( path ), AF_UNIX, 0, path );
	});
	it( "works for buffer paths", function() {
		let path = mkbuf( "héllo" );
		matchbuf( sa_unix( path ), AF_UNIX, 0, path );
	});
	it( "works for abstract namespace paths", function() {
		let path = mkbuf( 0, "foo", 0, 255, 0, 0 );
		matchbuf( sa_unix( path ), AF_UNIX, 0, path );
	});
});
