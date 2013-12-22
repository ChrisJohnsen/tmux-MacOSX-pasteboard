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

## Fine-Grained Usage

Instead of using `default-command` to “wrap” your top-level shells,
you can instead use the wrapper on just the tools that need it.

You might want to adopt this approach if part of your shell
initialization takes a long time to run: the `default-command` will
start your shell twice (once to process the `default-command`, and
once more for the final, interactive shell that is started by the
`default-command`). For example if your SHELL is *zsh*, then your
`.zshenv` (if you have one) will be run twice (the other
initialization files will only be run once).

For example, you could leave your shell “detached” and run
`reattach-to-user-namespace pbpaste` to read from the pasteboard. If
you take this approach, you may want to use a small script, shell
alias, or shell function to supply a shorter name for the wrapper
program (or wrap the individual commands—the
`reattach-to-user-namespace` *Homebrew* recipe has some support for
this).

You will also need to apply this fine-grained “wrapping” if you want
to have *tmux* directly run commands that need to be reattached.

For example, you might use bindings like the following to write
a *tmux* buffer to the OS X pasteboard or to paste the OS X
pasteboard into the current *tmux* pane.

    bind-key C-c run-shell 'tmux save-buffer - | reattach-to-user-namespace pbcopy'
    bind-key C-v run-shell 'reattach-to-user-namespace pbpaste | tmux load-buffer - \; paste-buffer -d'

Similarly, for the `copy-pipe` command (new in *tmux* 1.8):

    bind-key -t    vi-copy y   'reattach-to-user-namespace pbcopy'
    bind-key -t emacs-copy M-w 'reattach-to-user-namespace pbcopy'
