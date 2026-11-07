# expect: 4: undeclared variable
hold x <- 1
recipe f() >>
    show x
<<
