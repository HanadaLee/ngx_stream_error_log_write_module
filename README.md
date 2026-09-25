# Name

`ngx_stream_error_log_write_module` allows writing error log entries based on conditional expressions in nginx configuration files.

# Table of Content

- [Name](#name)
- [Table of Content](#table-of-content)
- [Status](#status)
- [Synopsis](#synopsis)
- [Installation](#installation)
- [Conditional syntax](#conditional-syntax)
- [Directives](#directives)
  - [error\_log\_write](#error_log_write)
- [Author](#author)
- [License](#license)

# Status

This Nginx module is currently considered experimental. Issues and PRs are welcome if you encounter any problems.

# Synopsis

```nginx
stream {
    error_log_write level=info "message=main test log";
    server {
        listen 12345;

        expr log_test !is_empty $remote_addr;
        when log_test {
            error_log_write "message=server test log";
        }
    }
}
```

# Installation

To use theses modules, configure your nginx branch with `--add-module=/path/to/ngx_stream_error_log_write_module`.

To enable named conditions, build `ngx_expr_module` and this module statically in the same nginx configuration.

# Conditional syntax

Conditional syntax is selected at compile time. With `ngx_expr_module`, place `error_log_write` inside a `stream` or `server` `when` block; `if=` and `if!=` are rejected. Without it, `when` is unavailable and legacy `if=`/`if!=` remain supported. `if=` matches a non-empty value other than `"0"`; `if!=` matches an empty value or `"0"`.

# Directives

## error_log_write

**Syntax:** *error_log_write [level=log_level] message=text;*

**Default:** *-*

**Context:** *stream, server, stream when, server when*

Writing a new error log. All error log entries are inherited unconditionally from the previous configuration level.

# Author

Hanada im@hanada.info

# License

This Nginx module is licensed under [BSD 2-Clause License](LICENSE).
