# Insertion Sort (from the proposal), sorting a small row in place
recipe isort(row num r, num n) >>
    for i from 1 until n >>
        hold key <- r[i]
        hold j <- i - 1
        while j >= 0 and r[j] > key >>
            r[j + 1] <- r[j]
            j <- j - 1
        <<
        r[j + 1] <- key
    <<
<<

hold a <- row num of 6
a[0] <- 42
a[1] <- 7
a[2] <- -3
a[3] <- 19
a[4] <- 7
a[5] <- 0
isort(a, count(a))
show a
