# save writes one element per line, replacing the file
hold r <- row num of 3
r[0] <- 7
r[2] <- -1
save r to "rt_saved.txt"
save "only this" to "rt_saved2.txt"
save 42 to "rt_saved2.txt"
show "saved"
