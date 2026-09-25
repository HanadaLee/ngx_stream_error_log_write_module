#!/usr/bin/perl

# Tests for legacy error_log_write filters without ngx_expr_module.

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

my $t = Test::Nginx->new()
	->has(qw/stream stream_return ngx_stream_error_log_write_module/)
	->plan(5);

$t->write_file_expand('nginx.conf', <<'EOF');

%%TEST_GLOBALS%%

daemon off;

events {
}

stream {
    %%TEST_GLOBALS_STREAM%%

    map $remote_addr $empty {
        default "";
    }

    server {
        listen     127.0.0.1:8080;
        error_log  %%TESTDIR%%/module.log info;

        error_log_write level=warn message=positive-hit if=$remote_addr;
        error_log_write level=warn message=positive-miss if=$empty;
        error_log_write level=info message=negative-hit if!=$empty;
        error_log_write level=info message=negative-miss if!=$remote_addr;

        return ok;
    }
}

EOF

$t->run();

###############################################################################

is(stream('127.0.0.1:' . port(8080))->read(), 'ok',
	'legacy filter session succeeds');

my $log = $t->read_file('module.log');

like($log, qr/\[warn\].*positive-hit/, 'if= logs a truthy value');
unlike($log, qr/positive-miss/, 'if= skips an empty value');
like($log, qr/\[info\].*negative-hit/, 'if!= logs an empty value');
unlike($log, qr/negative-miss/, 'if!= skips a truthy value');

###############################################################################
