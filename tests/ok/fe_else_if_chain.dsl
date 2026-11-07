# else-if chains and << else >> on one line
for i from 0 until 4 >>
    if i == 0 >>
        show "zero"
    << else if i == 1 >>
        show "one"
    << else if i == 2 >>
        show "two"
    << else >>
        show "many"
    <<
<<
