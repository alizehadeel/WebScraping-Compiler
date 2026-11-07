# expect: 5: arguments
recipe add(num a, num b) -> num >>
    give a + b
<<
hold s <- add(1)
