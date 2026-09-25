#!/usr/bin/perl

# Tests for ngx_stream_error_log_write_module with ngx_expr_module.

###############################################################################

use warnings;
use strict;

use Test::More;

BEGIN { use FindBin; chdir($FindBin::Bin); }

use lib 'lib';
use Test::Nginx;
use Test::Nginx::Stream qw/ stream /;

###############################################################################

select STDERR; $| = 1;
select STDOUT; $| = 1;

my $t = Test::Nginx->new()->has(qw/stream stream_return ngx_expr_module
	ngx_stream_error_log_write_module/)->plan(10);

$t->write_file_expand('nginx.conf', <<'EOF');

%%TEST_GLOBALS%%

daemon off;

events {
}

stream {
    %%TEST_GLOBALS_STREAM%%

    error_log_write level=notice message=stream:$server_port:$status;

    server {
        listen     127.0.0.1:8080;
        error_log  %%TESTDIR%%/module.log info;

        expr selected bool true;

        when selected {
            error_log_write level=warn message=conditional:$server_port;
        }

        error_log_write level=error message=server:$server_port;
        error_log_write level=info message=local:$protocol;

        return first;
    }

    server {
        listen     127.0.0.1:8081;
        error_log  %%TESTDIR%%/module.log info;

        expr selected bool false;

        when selected {
            error_log_write level=warn message=conditional:$server_port;
        }

        error_log_write level=error message=server:$server_port;

        return second;
    }
}

EOF

$t->run();

###############################################################################

is(response(8080), 'first', 'conditional session succeeds');
is(response(8081), 'second', 'unconditional session succeeds');

my $log = $t->read_file('module.log');
my ($first, $second) = (port(8080), port(8081));

like($log, qr/\[info\].*local:TCP/, 'entry uses configured info level');
like($log, qr/\[warn\].*conditional:$first/,
	'matching condition writes entry with variable');
unlike($log, qr/conditional:$second/,
	'non-matching condition does not write entry');
like($log, qr/\[error\].*server:$first/,
	'first server writes unconditional entry');
like($log, qr/\[error\].*server:$second/,
	'second server writes unconditional entry');
like($log, qr/\[notice\].*stream:$first:200/,
	'stream entry is inherited by first server');
like($log, qr/\[notice\].*stream:$second:200/,
	'stream entry evaluates final status');

my @local = $log =~ /local:TCP/g;
is(scalar @local, 1, 'local entry is written once');

###############################################################################

sub response {
	return stream('127.0.0.1:' . port(shift))->read();
}

###############################################################################
