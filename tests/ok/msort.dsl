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

hold data <- row num of 7
data[0] <- 38
data[1] <- 27
data[2] <- 43
data[3] <- 3
data[4] <- 9
data[5] <- 82
data[6] <- 10
hold tmp <- row num of count(data)
msort(data, tmp, 0, count(data) - 1)
for i from 0 until count(data) >>
    show data[i]
<<
