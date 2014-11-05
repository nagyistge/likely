Benchmark
---------
Likely functions tested against their [OpenCV](http://www.opencv.org) equivalents.

**[Results](https://s3.amazonaws.com/liblikely/benchmark.txt)**

### Fused multiply add
Compare to **[cv::Mat::convertTo](http://docs.opencv.org/2.4.8/modules/core/doc/basic_structures.html?highlight=convertto#mat-convertto)**.

    fused-multiply-add :=
      src :->
      {
        dst-type := src.type.floating
        dst := src.(imitate-size dst-type)
        (dst src) :=>
          (<- dst (+ (* src.dst-type 2.dst-type) 3.dst-type))
      }

### Binary threshold
Compare to **[cv::threshold(THRESHOLD_BINARY)](http://docs.opencv.org/2.4.8/modules/imgproc/doc/miscellaneous_transformations.html?highlight=threshold#threshold)**.

    binary-threshold :=
      src :->
      {
        dst := src.imitate
        (dst src) :=>
          (<- dst (src.type (threshold-binary src 127 1)))
      }