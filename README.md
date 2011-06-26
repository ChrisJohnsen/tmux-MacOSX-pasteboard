# Quick Summary

* Using the Mac OS X programs *pbpaste* and *pbcopy* under *tmux*
  does not work.  
  Other services and unpatched builds of *screen* are also affected.

* Certain undocumented, private API functions can fix the problem.

* Because the functions are private, undocumented, and unstable (one
  acquired an extra argument in 10.6), I think using a small wrapper
  program might be better than patching *tmux*.

Thus, my wrapper-based workaround:

1. Compile *reattach-to-user-namespace* from this repository.  
   Make it available in your PATH (or use the absolute pathname in
   the next step).

        make reattach-to-user-namespace &&
        cp reattach-to-user-namespace ~/bin

1. Configure *tmux* to use this wrapper program to start the shell
   for each new window.

    In `.tmux.conf`:

        set-option -g default-command "reattach-to-user-namespace -l zsh"

1. Restart your *tmux* server (or start a new one, or just
   reconfigure your existing one).

    To kill your existing *tmux* server (and everything running
    “inside” it!):

        tmux kill-server

1. Enjoy being able to use *pbpaste*, *pbcopy*, etc. in new shell
   windows.

# Purpose of These Programs

The programs in this repository were created to diagnose and enable
reliable access to the Mac OS X pasteboard for programs run under
[*tmux*][1] and unmodified versions of [*screen*][2].

[1]: http://tmux.sourceforge.net/
[2]: http://www.gnu.org/software/screen/

# Mac OS X Pasteboard Access Under *tmux* And *screen*

## The Problem

The most commonly reported broken behavior is that the [*pbcopy* and
*pbpaste*][3] command-line programs that come with Mac OS X fail to
function properly when run under *tmux* and (sometimes) under
“unpatched” *screen*.

Apple has patched their builds of *screen* (included with Mac OS X)
to fix the problem; the [*screen* “port”][4] in the [MacPorts][5]
system has [adopted][6] Apple’s *screen* patches.

Their *screen* patch allows (for example) the user to create
a *screen* session under a normal GUI login session and access the
pasteboard (inside the *screen* session) anytime that user is logged
into the GUI. Programs that are run in a session of “unpatched” *screen* will
only encounter the problem when the *screen* session outlives its
parent Mac OS X login session (e.g. a normal GUI login or an SSH
login).

Third-party programs (run under *tmux* or unpatched *screen*) are
also affected (e.g. non-GUI builds of [Vim][7] [7.3][8] can access
the pasteboard when compiled with the `+clipboard` feature).

[3]: http://developer.apple.com/library/mac/#documentation/Darwin/Reference/ManPages/man1/pbcopy.1.html
[4]: https://trac.macports.org/browser/trunk/dports/sysutils/screen/Portfile
[5]: http://www.macports.org/
[6]: https://trac.macports.org/browser/trunk/dports/sysutils/screen/files/patch-screen.c
[7]: http://www.vim.org/
[8]: http://vimhelp.appspot.com/version7.txt.html#added-7.3

## Why Pasteboard Breaks

### Access to the Mac OS X Pasteboard Service

The pasteboard service in Mac OS X is registered in a "bootstrap
namespace" (see Apple’s [TN2083][9]). The namespaces exist in
a hierarchy: “higher” namespaces include access to “lower”
namespaces. A process in a lower namespace can not access higher
namespaces. So, all process can access the lowest, “root” bootstrap
namespace, but only processes in a higher namespace can access that
namespace. Processes created as a part of a Mac OS X login session
are automatically included in the user’s “per-user” bootstrap
namespace. The pasteboard service is only available to processes in
the per-user bootstrap namespace.

[9]: http://developer.apple.com/library/mac/#technotes/tn2083/_index.html

### Interaction with *tmux*

*tmux* uses the *daemon(3)* library function when starting its
server process. In Mac OS X 10.5, Apple changed *daemon(3)* to move
the resulting process from its original bootstrap namespace to the
root bootstrap namespace. This means that the *tmux* server, and its
children, will automatically and uncontrollably lose access to what
would have been their original bootstrap namespace (i.e. the one
that has access to the pasteboard service).

### Interaction with Unpatched *screen*

The situation with *screen* is a bit different since it does not use
*daemon(3)*. Unpatched *screen*, and its children, only lose access
to the per-user bootstrap namespace when its parent login session
exits.

## Solution Space

Apple (and MacPorts) have already handled *screen*. Apple prevents
*screen* from losing access to the per-user bootstrap namespace by
“migrating to [the] background session” ([in 10.5][10] using
*\_vprocmgr\_move\_subset\_to\_user*) or “detach[ing] from console”
([in 10.6][11] using *\_vprocmgr\_detach\_from\_console*). For the
purposes of *screen*, both of these let the *screen* process access
the per-user bootstrap namespace even after the processes initial
Mac OS X login session has ended.

[10]: http://www.opensource.apple.com/source/screen/screen-12/patches/screen.c.diff
[11]: http://www.opensource.apple.com/source/screen/screen-19/screen/screen.c

### Patch *tmux*?

Ideally, we could port Apple’s patch to *tmux*. Practically, there
are problems with a direct port.

The undocumented, private function used in Apple’s 10.6 patch,
*\_vprocmgr\_detach\_from\_console*, is not effective if called before
*daemon(3)* (since it forcibly moves the process to the root
bootstrap namespace); if called after *daemon(3)*, it just returns
an error.

The undocumented, private function used in Apple’s 10.5 patch,
*\_vprocmgr\_move\_subset\_to\_user*, is also available in 10.6 (though
an extra parameter has been added to it in 10.6). Again, there is no
point in calling it before *daemon(3)*, but it is effective if
called after *daemon(3)*.

The functionality of *\_vprocmgr\_move\_subset\_to\_user* seems to be
a sort of superset of that of *\_vprocmgr\_detach\_from\_console* in
that both move to the `"Background"` session, but the former does
some extra work that can attach to a user namespace even if the
process has been previously moved out of it.

So, another approach that works is to call either the private
function after invoking a custom *daemon* that does not forcibly
move its resulting process to the root bootstrap namespace (*tmux*
even already has one).

The fact that the signature of *\_vprocmgr\_move\_subset\_to\_user*
changed between 10.5 and 10.6 is a strong indication that Apple sees
these functions as part of a private API that is liable to change or
become available in any (major?) release. It seems inappropriate to
ask upstream *tmux* to incorporate calls to functions such as these.
It might be appropriate for MacPorts to apply a patch to its port
though.

### Use a “Reattaching” Wrapper Program

While it would be nice to have the *tmux* server itself reattached
to the per-user bootstrap namespace, it is probably enough to
selectively reattach just some of its children. A small wrapper
could do the work of reattaching to the appropriate namespace and
then execing some other program that will (eventually) need
access to the per-user namespace.

Such a wrapper could be used to run *pbcopy*, *pbpaste*, *vim*, et
cetera. This would require the user to remember to use the wrapper
(or write scripts/shell-functions/aliases to always do it; or notice
it fail then re-run it under the wrapper).

A more automated solution that probably covers most of the problem
scenarios for most users would be to set *tmux*&rsquo;s `default-command`
option so that new windows start shells via the wrapper by default.
The major area this would not cover would be commands given directly
to `new-session` and `new-window` (there are some other commands
that start new children, but those are the major ones).

# Some New Programs For Your Consideration

## The Wrapper Program

The *reattach-to-user-namespace* program implements the “wrapper”
solution described above.

        reattach-to-user-namespace program args...

Its `-l` option causes it to rewrite the execed program’s `argv[0]` to
start with a dash (`-`). Most shells take this as a signal that they should
start as “login” shells.

        exec reattach-to-user-namespace -l "$SHELL"

In `.tmux.conf`:

        set-option -g default-command "reattach-to-user-namespace -l zsh"

## The Diagnostic Program

The *test* program was created to easily examine the effects and
interactions of some of the “functions of interest” (primarily
*daemon(3)*, and the private “vproc” functions).

Its arguments are interpreted as instructions to call various
functions and/or display some result.

Examples:

Emulate calling *pbpaste* under plain *tmux*:

        ./test daemon=sys system=pbpaste

Emulate a *tmux* patch that would automatically reattach to the user
namespace (also equivalent to using the wrapper program under an
unpatched *tmux*):

        ./test daemon=sys move-to-user=10.6 system=pbpaste

Emulate a *tmux* patch that uses compat/daemon.c and “detaches from
the console”:

        ./test daemon=ours deatch system=pbpaste

Demonstrate revocation of access to the per-user bootstrap namespace
when the Mac OS X login session ends:

        # while logged into the GUI
    
        # login session ends before pbpaste happens: failure
        cp /dev/null /tmp/f &&
        ssh localhost `pwd`/test \
          daemon=ours \
          msg=sleeping... sleep=1 msg='done\ sleeping' \
          system=pbpaste 2\> /tmp/f &&
        { cat /tmp/f; tail -f /tmp/f; }
    
        # pbpaste happens before login session ends: success
        cp /dev/null /tmp/f &&
        ssh localhost `pwd`/test \
          daemon=ours \
          msg=sleeping... msg='done\ sleeping' \
          system=pbpaste 2\> /tmp/f \; sleep 1 &&
        { cat /tmp/f; tail -f /tmp/f; }

Test workarounds to prevent the above end-of-login revocation:

        # while logged into the GUI
    
        # emulate tmux patched to move to the user namespace
        # or, equivalently, unpatched *tmux* and wrapper
        cp /dev/null /tmp/f &&
        ssh localhost `pwd`/test \
          daemon=sys \
          move-to-user=10.6 \
          msg=sleeping... sleep=1 msg='done\ sleeping' \
          system=pbpaste 2\> /tmp/f &&
        { cat /tmp/f; tail -f /tmp/f; }
    
        # emuate tmux patched to use compat/daemon + detach
        cp /dev/null /tmp/f &&
        ssh localhost `pwd`/test \
          daemon=ours \
          detach \
          msg=sleeping... sleep=1 msg='done\ sleeping' \
          system=pbpaste 2\> /tmp/f &&
        { cat /tmp/f; tail -f /tmp/f; }
