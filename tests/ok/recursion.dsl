recipe fact(num n) -> num >>
    if n <= 1 >>
        give 1
    <<
    give n * fact(n - 1)
<<
recipe fib(num n) -> num >>
    if n < 2 >>
        give n
    <<
    give fib(n - 1) + fib(n - 2)
<<
show fact(10)
show fib(15)
