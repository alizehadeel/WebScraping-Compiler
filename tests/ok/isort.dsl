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

hold data <- row num of 6
data[0] <- 5
data[1] <- 2
data[2] <- 9
data[3] <- 1
data[4] <- 5
data[5] <- 3
isort(data, count(data))
for i from 0 until count(data) >>
    show data[i]
<<
