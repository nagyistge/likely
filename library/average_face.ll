Average Face
------------
Compute the average face from a set of aligned faces [1].

    avg:= m:-> (=> (dst m)
    {
      j:= 0
      ($ j:= j:+ m t m.frames)
      j:/ m.frames
    } (1.frames))

    (average_face):= ():-> "data/lfw2".read.avg
    (average_face)

[1] http://www.openu.ac.il/home/hassner/data/lfwa/
