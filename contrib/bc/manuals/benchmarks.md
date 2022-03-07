# Benchmarks

The results of these benchmarks suggest that building this `bc` with
optimization at `-O3` with link-time optimization (`-flto`) will result in the
best performance. However, using `-march=native` can result in **WORSE**
performance.

*Note*: all benchmarks were run four times, and the fastest run is the one
shown. Also, `[bc]` means whichever `bc` was being run, and the assumed working
directory is the root directory of this repository. Also, this `bc` was at
version `3.0.0` while GNU `bc` was at version `1.07.1`, and all tests were
conducted on an `x86_64` machine running Gentoo Linux with `clang` `9.0.1` as
the compiler.

## Typical Optimization Level

These benchmarks were run with both `bc`'s compiled with the typical `-O2`
optimizations and no link-time optimization.

### Addition

The command used was:

```
tests/script.sh bc add.bc 1 0 1 1 [bc]
```

For GNU `bc`:

```
real 2.54
user 1.21
sys 1.32
```

For this `bc`:

```
real 0.88
user 0.85
sys 0.02
```

### Subtraction

The command used was:

```
tests/script.sh bc subtract.bc 1 0 1 1 [bc]
```

For GNU `bc`:

```
real 2.51
user 1.05
sys 1.45
```

For this `bc`:

```
real 0.91
user 0.85
sys 0.05
```

### Multiplication

The command used was:

```
tests/script.sh bc multiply.bc 1 0 1 1 [bc]
```

For GNU `bc`:

```
real 7.15
user 4.69
sys 2.46
```

For this `bc`:

```
real 2.20
user 2.10
sys 0.09
```

### Division

The command used was:

```
tests/script.sh bc divide.bc 1 0 1 1 [bc]
```

For GNU `bc`:

```
real 3.36
user 1.87
sys 1.48
```

For this `bc`:

```
real 1.61
user 1.57
sys 0.03
```

### Power

The command used was:

```
printf '1234567890^100000; halt\n' | time -p [bc] -q > /dev/null
```

For GNU `bc`:

```
real 11.30
user 11.30
sys 0.00
```

For this `bc`:

```
real 0.73
user 0.72
sys 0.00
```

### Scripts

[This file][1] was downloaded, saved at `../timeconst.bc` and the following
patch was applied:

```
--- ../timeconst.bc	2018-09-28 11:32:22.808669000 -0600
+++ ../timeconst.bc	2019-06-07 07:26:36.359913078 -0600
@@ -110,8 +110,10 @@
 
 		print "#endif /* KERNEL_TIMECONST_H */\n"
 	}
-	halt
 }
 
-hz = read();
-timeconst(hz)
+for (i = 0; i <= 50000; ++i) {
+	timeconst(i)
+}
+
+halt
```

The command used was:

```
time -p [bc] ../timeconst.bc > /dev/null
```

For GNU `bc`:

```
real 16.71
user 16.06
sys 0.65
```

For this `bc`:

```
real 13.16
user 13.15
sys 0.00
```

Because this `bc` is faster when doing math, it might be a better comparison to
run a script that is not running any math. As such, I put the following into
`../test.bc`:

```
for (i = 0; i < 100000000; ++i) {
	y = i
}

i
y

halt
```

The command used was:

```
time -p [bc] ../test.bc > /dev/null
```

For GNU `bc`:

```
real 16.60
user 16.59
sys 0.00
```

For this `bc`:

```
real 22.76
user 22.75
sys 0.00
```

I also put the following into `../test2.bc`:

```
i = 0

while (i < 100000000) {
	i += 1
}

i

halt
```

The command used was:

```
time -p [bc] ../test2.bc > /dev/null
```

For GNU `bc`:

```
real 17.32
user 17.30
sys 0.00
```

For this `bc`:

```
real 16.98
user 16.96
sys 0.01
```

It seems that the improvements to the interpreter helped a lot in certain cases.

Also, I have no idea why GNU `bc` did worse when it is technically doing less
work.

## Recommended Optimizations from `2.7.0`

Note that, when running the benchmarks, the optimizations used are not the ones
I recommended for version `2.7.0`, which are `-O3 -flto -march=native`.

This `bc` separates its code into modules that, when optimized at link time,
removes a lot of the inefficiency that comes from function overhead. This is
most keenly felt with one function: `bc_vec_item()`, which should turn into just
one instruction (on `x86_64`) when optimized at link time and inlined. There are
other functions that matter as well.

I also recommended `-march=native` on the grounds that newer instructions would
increase performance on math-heavy code. We will see if that assumption was
correct. (Spoiler: **NO**.)

When compiling both `bc`'s with the optimizations I recommended for this `bc`
for version `2.7.0`, the results are as follows.

### Addition

The command used was:

```
tests/script.sh bc add.bc 1 0 1 1 [bc]
```

For GNU `bc`:

```
real 2.44
user 1.11
sys 1.32
```

For this `bc`:

```
real 0.59
user 0.54
sys 0.05
```

### Subtraction

The command used was:

```
tests/script.sh bc subtract.bc 1 0 1 1 [bc]
```

For GNU `bc`:

```
real 2.42
user 1.02
sys 1.40
```

For this `bc`:

```
real 0.64
user 0.57
sys 0.06
```

### Multiplication

The command used was:

```
tests/script.sh bc multiply.bc 1 0 1 1 [bc]
```

For GNU `bc`:

```
real 7.01
user 4.50
sys 2.50
```

For this `bc`:

```
real 1.59
user 1.53
sys 0.05
```

### Division

The command used was:

```
tests/script.sh bc divide.bc 1 0 1 1 [bc]
```

For GNU `bc`:

```
real 3.26
user 1.82
sys 1.44
```

For this `bc`:

```
real 1.24
user 1.20
sys 0.03
```

### Power

The command used was:

```
printf '1234567890^100000; halt\n' | time -p [bc] -q > /dev/null
```

For GNU `bc`:

```
real 11.08
user 11.07
sys 0.00
```

For this `bc`:

```
real 0.71
user 0.70
sys 0.00
```

### Scripts

The command for the `../timeconst.bc` script was:

```
time -p [bc] ../timeconst.bc > /dev/null
```

For GNU `bc`:

```
real 15.62
user 15.08
sys 0.53
```

For this `bc`:

```
real 10.09
user 10.08
sys 0.01
```

The command for the next script, the `for` loop script, was:

```
time -p [bc] ../test.bc > /dev/null
```

For GNU `bc`:

```
real 14.76
user 14.75
sys 0.00
```

For this `bc`:

```
real 17.95
user 17.94
sys 0.00
```

The command for the next script, the `while` loop script, was:

```
time -p [bc] ../test2.bc > /dev/null
```

For GNU `bc`:

```
real 14.84
user 14.83
sys 0.00
```

For this `bc`:

```
real 13.53
user 13.52
sys 0.00
```

## Link-Time Optimization Only

Just for kicks, let's see if `-march=native` is even useful.

The optimizations I used for both `bc`'s were `-O3 -flto`.

### Addition

The command used was:

```
tests/script.sh bc add.bc 1 0 1 1 [bc]
```

For GNU `bc`:

```
real 2.41
user 1.05
sys 1.35
```

For this `bc`:

```
real 0.58
user 0.52
sys 0.05
```

### Subtraction

The command used was:

```
tests/script.sh bc subtract.bc 1 0 1 1 [bc]
```

For GNU `bc`:

```
real 2.39
user 1.10
sys 1.28
```

For this `bc`:

```
real 0.65
user 0.57
sys 0.07
```

### Multiplication

The command used was:

```
tests/script.sh bc multiply.bc 1 0 1 1 [bc]
```

For GNU `bc`:

```
real 6.82
user 4.30
sys 2.51
```

For this `bc`:

```
real 1.57
user 1.49
sys 0.08
```

### Division

The command used was:

```
tests/script.sh bc divide.bc 1 0 1 1 [bc]
```

For GNU `bc`:

```
real 3.25
user 1.81
sys 1.43
```

For this `bc`:

```
real 1.27
user 1.23
sys 0.04
```

### Power

The command used was:

```
printf '1234567890^100000; halt\n' | time -p [bc] -q > /dev/null
```

For GNU `bc`:

```
real 10.50
user 10.49
sys 0.00
```

For this `bc`:

```
real 0.72
user 0.71
sys 0.00
```

### Scripts

The command for the `../timeconst.bc` script was:

```
time -p [bc] ../timeconst.bc > /dev/null
```

For GNU `bc`:

```
real 15.50
user 14.81
sys 0.68
```

For this `bc`:

```
real 10.17
user 10.15
sys 0.01
```

The command for the next script, the `for` loop script, was:

```
time -p [bc] ../test.bc > /dev/null
```

For GNU `bc`:

```
real 14.99
user 14.99
sys 0.00
```

For this `bc`:

```
real 16.85
user 16.84
sys 0.00
```

The command for the next script, the `while` loop script, was:

```
time -p [bc] ../test2.bc > /dev/null
```

For GNU `bc`:

```
real 14.92
user 14.91
sys 0.00
```

For this `bc`:

```
real 12.75
user 12.75
sys 0.00
```

It turns out that `-march=native` can be a problem. As such, I have removed the
recommendation to build with `-march=native`.

## Recommended Compiler

When I ran these benchmarks with my `bc` compiled under `clang` vs. `gcc`, it
performed much better under `clang`. I recommend compiling this `bc` with
`clang`.

[1]: https://github.com/torvalds/linux/blob/master/kernel/time/timeconst.bc
