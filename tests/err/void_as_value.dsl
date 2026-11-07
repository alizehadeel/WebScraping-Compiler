# expect: 5: returns nothing
recipe hello() >>
    show 1
<<
hold a <- hello()
