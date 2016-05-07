# sce-chess
Stands for "Sumer's Chess Engine" - very original :). 

Core board representation was first inspired by the Stockfish chess engine, while the searching primarily consists of alpha-beta tree search with some 
null-move pruning and other "tricks" to reduce the search tree. 

Rated at roughly ~2045 ELO, with a margin of error of ~1.8%, using cutechess-cli and playing against a handful of other engines. 

Has book creation and reading functionality, not to mention PGN reading and writing, but also implements a form 
of so-called "book learning" (like machine learning, but a better comparison would be a genetic algorithm).

Despite being uploaded to Github in May 2016, I worked on this project for the better part of the 2014 year, creating
this in about January and working fairly often and in earnest until maybe August, then on-and-off until now.
