recipe fill(row num r, num v) >>
    for i from 0 until count(r) >>
        r[i] <- v + i
    <<
<<
hold data <- row num of 4
fill(data, 10)
hold alias <- data
alias[0] <- 99
for i from 0 until count(data) >>
    show data[i]
<<
