# grab as num and as text on the real shop page
open "../../examples/shop.html"
hold p <- grab "span.price" as num
show p
hold t <- grab "div.product h2"
show count(t)
show t[0]
show count(grab "span.nothing")
show grab "#main h1"
show grab ".footer .note"
