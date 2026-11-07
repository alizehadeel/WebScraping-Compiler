# expect: 6: preceded by an open
hold flag <- 1
if flag >>
    open "shop.html"
<<
hold p <- grab "span.price" as num
