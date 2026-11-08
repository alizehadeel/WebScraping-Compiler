# Phase 2 core codegen: division by -1, short-circuit, else-if, text ==, for, while, grab, show/save of a row
hold x <- 7
hold y <- -3
show x / y
show x % y
show -9223372036854775807 - 1 / -1
show (0 - 9223372036854775807 - 1) / -1
show 5 % -1
show x > 2 and y < 0
show 0 or 0
if x == 7 >>
    show "seven"
<< else if x == 8 >>
    show "eight"
<< else >>
    show "other"
<<
hold s <- "a\"b"
show s == "a\"b"
show s != "ab"
for i from 0 until 3 >>
    hold z <- i * 10
    show z
<<
hold k <- 0
while k < 3 and not (k == 2) >>
    k <- k + 1
<<
show k
open "../../examples/shop.html"
hold p <- grab "span.price" as num
show p
save p to "cg_core.txt"
