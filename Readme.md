# Watermaking by Motion Vector
The JM19.0 codec is modified to able using motion vector to embedded data. Data is inserted when encoding specified stream, and the one can be got by decoding the stream.

# How to use
## Encoder is added 3 input parameters.
- p WaterMarkFile="input embed data file"
- p Threshold=threshold value of motion vector difference, which is changed (mvd_x^2 + mvd_y^2 <= threshold)
- p InsertMode=insertmode 
  - 0: no insert -> result is same original JM19
  - 1: only modified motion vector forward
  - 2: only modify mv backward
  - 3: both forward and backward

## Decoder is added 2 input parameters.
- p WaterMarkFile="output file for storing embed data, which is get from bitstream"
- p Threshold=threshold value of motion vector difference, which is changed (mvd_x^2 + mvd_y^2 <= threshold)

## Example 
Using by modifing base on default config encoder.cfg & decoder.cfg.
### For encoder, add below parameter to encoder.cfg
> ReconFile             = "test_rec.yuv"       # Reconstruction YUV file  
> OutputFile            = "test.264"           # Bitstream  
> WaterMarkFile         = "wm_file_enc.txt"    # Bitstream  
> Threshold	      = 2  
> InsertMode	      = 3  

- Command: ./lencod.exe -f encoder.cfg  

### For decoder, add below parameter to decoder.cfg
> InputFile             = "test.264"       # H.264/AVC coded bitstream  
> OutputFile            = "test_dec.yuv"   # Output file, YUV/RGB  
> WaterMarkFile         = "wm_file_dec.txt"    # Bitstream  

- Command: ./ldecod.exe -f decoder.cfg  

## Confirm result
- Local decoded result at encoder phase (test_rec.yuv) is identical with decoded result at decoder phase( test_dec.yuv)
- Input watermark file in encoder is same as output watermark file at decoder.

# Reference
- The agorithm is implemented base on [JM Reference Software](http://iphome.hhi.de/suehring/tml)
- Refer https://vcostudy.com/2018/04/25/che-dau-du-lieu-bang-motion-vector/
