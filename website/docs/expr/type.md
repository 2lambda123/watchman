---
title: type
category: Expression Terms
---

Evaluates as true if the type of the file matches that specified by the second
argument; this matches regular files:

    ["type", "f"]

Possible types are:

- **b**: block special file
- **c**: character special file
- **d**: directory
- **f**: regular file
- **p**: named pipe (fifo)
- **l**: symbolic link
- **s**: socket
- **D**: Solaris Door
- **?**: An unknown file type
