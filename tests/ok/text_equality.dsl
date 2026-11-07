hold a <- "hello"
hold b <- "hel"
if a == "hello" >>
    show 1
<<
if a != b >>
    show 2
<<
if a == b >>
    show 3
<< else >>
    show 4
<<
