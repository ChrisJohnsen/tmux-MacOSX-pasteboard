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
available (e.g. same files on both OS X and Linux). Starting with
*tmux* 1.9, it is safe to use `if-shell` to conditionally configure
the use of the wrapper program:

    if-shell 'test "$(uname -s)" = Darwin' 'set-option -g default-command "exec reattach-to-user-namespace -l zsh"'

Or, if you have other platform specific configuration, you can
conditionally source another file.

In `.tmux.conf`:

    if-shell 'test "$(uname -s)" = Darwin' 'source-file ~/.tmux-osx.conf'

Then in `.tmux-osx.conf`, include the `default-command` setting and
any other bits of OS X configuration (e.g. the buffer-to-pasteboard
bindings shown below):

    set-option -g default-command 'exec reattach-to-user-namespace -l zsh'

With *tmux* 1.8 and earlier, the above configuration has a race
condition (the `if-shell` command is run in the background) and/or
does not affect the initial session/window/pane. Instead, the basic
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
you can instead use the wrapper on just the tools that need it (see
“Beyond Pasteboard Access” (below) for other commands that may also
need the wrapper).

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

    bind-key -t    vi-copy y   copy-pipe 'reattach-to-user-namespace pbcopy'
    bind-key -t emacs-copy M-w copy-pipe 'reattach-to-user-namespace pbcopy'



# Beyond Pasteboard Access

Because the fix applied by the wrapper program is not limited to
just pasteboard access, there are other bugs/issues/problems that
come up when running under *tmux* that the wrapper can help
alleviate.

* `nohup`

    The *nohup* program aborts if it cannot “detach from console”.
    Normally, processes running “inside” *tmux* (on the other side
    of a daemon(3) call) are already detached; the wrapper
    reattaches so that *nohup* can successfully detach itself.

    *nohup* does generate an error message if it aborts due to
    failing to detach, but it happens after the output has been
    redirected so it ends up in the `nohup.out` file (or wherever
    you sent stdout):

        nohup: can't detach from console: Undefined error: 0

    References: [problem using nohup within tmux on OSX 10.6][ml nohup]

[ml nohup]: http://thread.gmane.org/gmane.comp.terminal-emulators.tmux.user/4450

* The `export`, `getenv`, `setenv` subcommands of `launchctl`

    Notably, the `setenv` subcommand will issue an error when it is
    run from a “detached” context:

        launch_msg("SetUserEnvironment"): Socket is not connected

    The `getenv` and `export` commands will simply not have access
    to some variables that have otherwise been configured in the
    user’s *launchd* instance.

    References: [tmux “Socket is not connected” error on OS X Lion][so setenv]

[so setenv]: http://stackoverflow.com/q/10193561/193688

* `subl -w` `subl --wait`

[su subl]: http://superuser.com/q/522055/14827
[so subl]: http://stackoverflow.com/q/13917095/193688

    The *subl* command from *Sublime Text* can be used to open files
    in the GUI window from a shell command line. The `-w` (or
    `--wait`) option tells it to wait for the file to be closed in
    the editor before exiting the `subl` command.

    Whatever mechanism *Sublime Text* uses to coordinate between the
    `subl` instance and the main *Sublime Text* instance is affected
    by being “detached”. The result is that `subl -w` commands
    issued inside *tmux* will not exit after the file is closed in
    the GUI. The wrapper lets the `subl` command successfully
    synchronize with the GUI instance.

    References: ['subl -w' doesn't ever un-block when running under tmux on OS X][su subl] and [subl --wait doesn't work within tmux][so subl]

* Retina rendering of apps launched under *tmux* (10.9 Mavericks)

    From feedback in [issue #22][issue 22]:

[issue 22]: https://github.com/ChrisJohnsen/tmux-MacOSX-pasteboard/issues/22

    > Under Mavericks, applications launched from inside a tmux
    > session cannot enable retina rendering (for reasons I don't
    > understand -- this worked under Mountain Lion). If the shell
    > inside the tmux session is reattached to the user namespace,
    > then applications launched from the reattached shell do enable
    > retina rendering when appropriate.

* `ABAddressBook` (via custom program)

     [Issue #43][issue 43] mentions a custom Swift program using
     `ABAddressBook` that does not work under *tmux* unless run with
     the wrapper (tested on Yosemite).

[issue 43]: https://github.com/ChrisJohnsen/tmux-MacOSX-pasteboard/issues/43

* MacVim using a font from ~/Library/Fonts for `guifont`

    [Issue #54][issue 54] mentions MacVim (8.0; possibly older
    versions, too) not being able to use a font from ~/Library/Fonts
    for its `guifont` setting when run under *tmux* unless run with
    the wrapper (tested under Sierra).

[issue 54]: https://github.com/ChrisJohnsen/tmux-MacOSX-pasteboard/issues/54

* `curl` using certificates in Keychain to verify peer

    > `--cacert`
    > (iOS and macOS only) If curl is built against Secure Transport [...] then
    > curl will use the certificates in the system and user Keychain to verify
    > the peer, which is the preferred method of verifying the peer's
    > certificate chain.
    
    As of macOS Sierra, under tmux without `reattach-to-user-namespace`, `curl`
    will fail to verify peers against Keychain-stored certificates.

* `ssh` - Keychain access for Secure Shell

    On macOS Sierra *tmux* can only add your SSH keys to `ssh-agent`
    with `reattach-to-user-namespace`,
    see ["macOS Sierra without SSH-passphrase"](https://youtu.be/w5iZkhlg24M)
    on YouTube.

There may also be other contexts (aside from “inside *tmux*”) where
these same problems occur, but I have not yet heard of any.
