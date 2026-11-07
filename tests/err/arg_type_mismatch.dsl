# expect: 5: argument 1
recipe double(num x) -> num >>
    give x * 2
<<
hold a <- double("hi")
