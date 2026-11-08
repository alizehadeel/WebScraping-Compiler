recipe f(num x) -> num >>
    if x > 0 >>
        give 1
        show "never"
    <<
    give 0
    show "never"
    show "never either"
<<
show f(5)
