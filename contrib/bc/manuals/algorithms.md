# Algorithms

This `bc` uses the math algorithms below:

### Addition

This `bc` uses brute force addition, which is linear (`O(n)`) in the number of
digits.

### Subtraction

This `bc` uses brute force subtraction, which is linear (`O(n)`) in the number
of digits.

### Multiplication

This `bc` uses two algorithms: [Karatsuba][1] and brute force.

Karatsuba is used for "large" numbers. ("Large" numbers are defined as any
number with `BC_NUM_KARATSUBA_LEN` digits or larger. `BC_NUM_KARATSUBA_LEN` has
a sane default, but may be configured by the user.) Karatsuba, as implemented in
this `bc`, is superlinear but subpolynomial (bounded by `O(n^log_2(3))`).

Brute force multiplication is used below `BC_NUM_KARATSUBA_LEN` digits. It is
polynomial (`O(n^2)`), but since Karatsuba requires both more intermediate
values (which translate to memory allocations) and a few more additions, there
is a "break even" point in the number of digits where brute force multiplication
is faster than Karatsuba. There is a script (`$ROOT/karatsuba.py`) that will
find the break even point on a particular machine.

***WARNING: The Karatsuba script requires Python 3.***

### Division

This `bc` uses Algorithm D ([long division][2]). Long division is polynomial
(`O(n^2)`), but unlike Karatsuba, any division "divide and conquer" algorithm
reaches its "break even" point with significantly larger numbers. "Fast"
algorithms become less attractive with division as this operation typically
reduces the problem size.

While the implementation of long division may appear to use the subtractive
chunking method, it only uses subtraction to find a quotient digit. It avoids
unnecessary work by aligning digits prior to performing subtraction and finding
a starting guess for the quotient.

Subtraction was used instead of multiplication for two reasons:

1.	Division and subtraction can share code (one of the less important goals of
	this `bc` is small code).
2.	It minimizes algorithmic complexity.

Using multiplication would make division have the even worse algorithmic
complexity of `O(n^(2*log_2(3)))` (best case) and `O(n^3)` (worst case).

### Power

This `bc` implements [Exponentiation by Squaring][3], which (via Karatsuba) has
a complexity of `O((n*log(n))^log_2(3))` which is favorable to the
`O((n*log(n))^2)` without Karatsuba.

### Square Root

This `bc` implements the fast algorithm [Newton's Method][4] (also known as the
Newton-Raphson Method, or the [Babylonian Method][5]) to perform the square root
operation. Its complexity is `O(log(n)*n^2)` as it requires one division per
iteration.

### Sine and Cosine (`bc` Only)

This `bc` uses the series

```
x - x^3/3! + x^5/5! - x^7/7! + ...
```

to calculate `sin(x)` and `cos(x)`. It also uses the relation

```
cos(x) = sin(x + pi/2)
```

to calculate `cos(x)`. It has a complexity of `O(n^3)`.

**Note**: this series has a tendency to *occasionally* produce an error of 1
[ULP][6]. (It is an unfortunate side effect of the algorithm, and there isn't
any way around it; [this article][7] explains why calculating sine and cosine,
and the other transcendental functions below, within less than 1 ULP is nearly
impossible and unnecessary.) Therefore, I recommend that users do their
calculations with the precision (`scale`) set to at least 1 greater than is
needed.

### Exponentiation (`bc` Only)

This `bc` uses the series

```
1 + x + x^2/2! + x^3/3! + ...
```

to calculate `e^x`. Since this only works when `x` is small, it uses

```
e^x = (e^(x/2))^2
```

to reduce `x`. It has a complexity of `O(n^3)`.

**Note**: this series can also produce errors of 1 ULP, so I recommend users do
their calculations with the precision (`scale`) set to at least 1 greater than
is needed.

### Natural Logarithm (`bc` Only)

This `bc` uses the series

```
a + a^3/3 + a^5/5 + ...
```

(where `a` is equal to `(x - 1)/(x + 1)`) to calculate `ln(x)` when `x` is small
and uses the relation

```
ln(x^2) = 2 * ln(x)
```

to sufficiently reduce `x`. It has a complexity of `O(n^3)`.

**Note**: this series can also produce errors of 1 ULP, so I recommend users do
their calculations with the precision (`scale`) set to at least 1 greater than
is needed.

### Arctangent (`bc` Only)

This `bc` uses the series

```
x - x^3/3 + x^5/5 - x^7/7 + ...
```

to calculate `atan(x)` for small `x` and the relation

```
atan(x) = atan(c) + atan((x - c)/(1 + x * c))
```

to reduce `x` to small enough. It has a complexity of `O(n^3)`.

**Note**: this series can also produce errors of 1 ULP, so I recommend users do
their calculations with the precision (`scale`) set to at least 1 greater than
is needed.

### Bessel (`bc` Only)

This `bc` uses the series

```
x^n/(2^n * n!) * (1 - x^2 * 2 * 1! * (n + 1)) + x^4/(2^4 * 2! * (n + 1) * (n + 2)) - ...
```

to calculate the bessel function (integer order only).

It also uses the relation

```
j(-n,x) = (-1)^n * j(n,x)
```

to calculate the bessel when `x < 0`, It has a complexity of `O(n^3)`.

**Note**: this series can also produce errors of 1 ULP, so I recommend users do
their calculations with the precision (`scale`) set to at least 1 greater than
is needed.

### Modular Exponentiation (`dc` Only)

This `dc` uses the [Memory-efficient method][8] to compute modular
exponentiation. The complexity is `O(e*n^2)`, which may initially seem
inefficient, but `n` is kept small by maintaining small numbers. In practice, it
is extremely fast.

[1]: https://en.wikipedia.org/wiki/Karatsuba_algorithm
[2]: https://en.wikipedia.org/wiki/Long_division
[3]: https://en.wikipedia.org/wiki/Exponentiation_by_squaring
[4]: https://en.wikipedia.org/wiki/Newton%27s_method#Square_root_of_a_number
[5]: https://en.wikipedia.org/wiki/Methods_of_computing_square_roots#Babylonian_method
[6]: https://en.wikipedia.org/wiki/Unit_in_the_last_place
[7]: https://people.eecs.berkeley.edu/~wkahan/LOG10HAF.TXT
[8]: https://en.wikipedia.org/wiki/Modular_exponentiation#Memory-efficient_method
