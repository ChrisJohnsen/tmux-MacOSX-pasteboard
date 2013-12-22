# Basic Configuration

The basic configuration is to set *tmux*’s `default-command` so that
your interactive shell ends up reattached to the user bootstrap
namespace:

    set-option -g default-command 'reattach-to-user-namespace -l zsh'

Since the “attachment status” is inherited by child processes, this
configuration will ensure that all the commands started from your
shell will also be properly attached.

# Configuration Alternatives

## Cross-Platform Conditional Usage

Some users like to share identical configuration files (including
`.tmux.conf`) with systems where the wrapper program is not
available (e.g. same files on both OS X and Linux). The basic
`default-command` can be extended with a bit of shell code to
conditionally use the wrapper only where it is present:

    set-option -g default-command 'command -v reattach-to-user-namespace >/dev/null && exec reattach-to-user-namespace -l "$SHELL" || exec "$SHELL"'

The `default-command` will be run using your `default-shell` (which
defaults to SHELL, your login shell, or `/bin/sh`). This particular
code should work on most systems, but it may fail if the effective
shell is not POSIX compliant (old-style `/bin/sh`, a *csh* variant,
*fish*, etc.). If one of your systems does not understand `command
-v` (or if it does something unrelated that returns the wrong exit
value), then you might try using `which` instead. Exotic shells may
also require different syntax.
