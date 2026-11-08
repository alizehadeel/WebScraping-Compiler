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

open "../../examples/shop.html"
hold prices <- grab "span.price" as num
hold n <- count(prices)
hold tmp <- row num of n
msort(prices, tmp, 0, n - 1)
# prices are now ascending, so the cheap ones come first
hold k <- 0
while k < n and prices[k] < 500 >>
    k <- k + 1
<<
hold cheap <- row num of k
for i from 0 until k >>
    cheap[i] <- prices[i]
<<
save cheap to "cheap_prices.txt"
show k
