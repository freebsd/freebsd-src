# "scale" not set correctly with -l when first command is a syntax error

## `mathieu`

I just hit a (small and unlikely to be triggered) problem when using the `-l` flag:

```
$ bc -l
>>> 2+; # or any other syntax error it seems

Parse error: bad expression
    <stdin>:1

>>> l(1000)
6
>>> scale
0
```

The math library still gets loaded but `scale` doesn't get set (or gets reset)?

The syntax error has to be on the first command and other kinds of errors (like say a divide by zero) don't seem to cause the problem.

## `gavin`

Hmm...let me investigate this and get back to you. This does seem like a bug.

## `gavin`

I'm not seeing the behavior. Can you send me the output of `bc -v`?

## `gavin`

I should also ask: what OS are you on? What version? What compiler did you use? Did you install from a package?

Basically, send me as much info as you can. I would appreciate it.

## `mathieu`

Oh sorry yeah I should've given more details.

That's on FreeBSD with the base system's bc, built with the default base compiler.

On recent 12.2-STABLE:

```
$ bc -v
bc 4.0.1
Copyright (c) 2018-2021 Gavin D. Howard and contributors
Report bugs at: https://git.yzena.com/gavin/bc

This is free software with ABSOLUTELY NO WARRANTY.
```

Your bc is not default on 12.X yet but I enabled it with WITH_GH_BC=yes in /etc/src.conf to try it out.

And on somewhat less recent 14-CURRENT:

```
$ bc -v
bc 4.0.0
Copyright (c) 2018-2021 Gavin D. Howard and contributors
Report bugs at: https://git.yzena.com/gavin/bc

This is free software with ABSOLUTELY NO WARRANTY.
```

Both amd64. Happens every time on both.

I could give it a try on 13-STABLE too if that helps but I'd need to reboot something.

The syntax error really has to be the FIRST input, even entering an empty line before it makes the problem not happen.

I thought it could be an editline(3) problem since some programs end up using that pretty much only on the BSDs it seems, but that's not it.

## `gavin`

Yeah, my `bc` uses a custom history implementation, so it could be mine, but not `editline(3)`.

I will pull up a FreeBSD VM and check it out. Sorry for the wait.

## `gavin`

I have confirmed the bug on FreeBSD with the port at version `4.0.1`. Since it is the port, and not the system one, I think the problem may lie with some incompatibility between my history implementation and what FreeBSD provides.

This one may take me a long time to debug because I have to do it manually in the VM. Thank you for your patience.

## `gavin`

I found the problem!

It is fixed in `299a4fd353`, but if you can pull that down and test, I would appreciate it.

I will put out a release as soon as I can, and my FreeBSD contact will probably update 14-CURRENT soon thereafter. He will also update the port, but the version in 12.2 may not be updated for a bit.

Feel free to reopen if the fix does not work for you.

## `mathieu`

Oof... yeah makes sense that an rc file could interfere with this.

Yes, that fixes it here too. With that diff applied on both 12.2-STABLE and 14-CURRENT's versions. And everything else seems to still work fine too.

Thanks for fixing this! You'd think it's really hard to trigger but I do enough typos and I start bc (with an alias with -l) often enough to make a quick calculation that I hit it twice and had decimals mysteriously missing before I started trying to reproduce it.
