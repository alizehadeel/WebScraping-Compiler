# expect: 3: contains no digits
open "../../examples/news.html"
hold n <- grab "div.story h2" as num
