# Collect headlines as text, skipping advertisement blocks
open "news.html"
hold titles <- grab "div.story h2"
show count(titles)
for i from 0 until count(titles) >>
    if titles[i] != "Advertisement" >>
        show titles[i]
    <<
<<
save titles to "headlines.txt"
