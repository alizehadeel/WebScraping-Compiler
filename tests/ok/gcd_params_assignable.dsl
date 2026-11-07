recipe gcd(num a, num b) -> num >>
    while b != 0 >>
        hold t <- b
        b <- a % b
        a <- t
    <<
    give a
<<
show gcd(48, 18)
show gcd(17, 5)
