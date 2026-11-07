recipe grade(num s) -> text >>
    if s >= 90 >>
        give "A"
    << else if s >= 80 >>
        give "B"
    << else >>
        give "C"
    <<
<<
show grade(95)
show grade(85)
show grade(10)
