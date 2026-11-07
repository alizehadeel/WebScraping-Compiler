recipe pos(num x) -> num >>
    if x > 0 >>
        give x
    <<
<<
recipe name(num x) -> text >>
<<
recipe none(num x) -> row num >>
<<
show pos(5)
show pos(-5)
show name(1)
show count(none(1))
