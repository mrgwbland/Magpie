> Ooo shiny!

Magpie is a UCI chess engine that plays without searching any nodes other than the root node, therefore making it a 0-ply chess engine. \
It only searches nodes to check for terminal positions, otherwise it simply evaluates moves. What this means is that through SEE and heuristics it evaluates every move at the root node to determine what it thinks is the best move. For those who are familiar with chess engines, this is essentially a work into extreme move ordering, but I shouldn't imagine it would yield any use for ordinary engines as the compute time for this engine is essentially irrelevant, as it explores one node only; the methods are likely to be far to computationally heavy for practical use.\
In practice this engine is of course a weak player but plays almost instantly, and can be fun to play against. This engine can be considered somewhat finalised unless I want to go into the application of neural networks. \
It is not yet rated but I will update here if it does. As it does not think recursively it will play identically no matter the time control and is thus the best in the most extreme short time controls. \
As it evaluates moves rather than positions it is unsuitable for analysis
