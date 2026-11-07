# Merge Sort (from the proposal): recursion and a temporary row
recipe merge(row num a, row num tmp, num lo, num mid, num hi) >>
    hold i <- lo
    hold j <- mid + 1
    hold k <- lo
    while i <= mid and j <= hi >>
        if a[i] <= a[j] >>
            tmp[k] <- a[i]
            i <- i + 1
        << else >>
            tmp[k] <- a[j]
            j <- j + 1
        <<
        k <- k + 1
    <<
    while i <= mid >>
        tmp[k] <- a[i]
        i <- i + 1
        k <- k + 1
    <<
    while j <= hi >>
        tmp[k] <- a[j]
        j <- j + 1
        k <- k + 1
    <<
    for t from lo until hi + 1 >>
        a[t] <- tmp[t]
    <<
<<

recipe msort(row num a, row num tmp, num lo, num hi) >>
    if lo < hi >>
        hold mid <- (lo + hi) / 2
        msort(a, tmp, lo, mid)
        msort(a, tmp, mid + 1, hi)
        merge(a, tmp, lo, mid, hi)
    <<
<<

hold a <- row num of 7
a[0] <- 38
a[1] <- 27
a[2] <- 43
a[3] <- 3
a[4] <- 9
a[5] <- 82
a[6] <- 10
hold tmp <- row num of count(a)
msort(a, tmp, 0, count(a) - 1)
show a
