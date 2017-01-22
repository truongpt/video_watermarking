1. For encoder: add 3 input parameter.
-p WaterMarkFile="input embed data file"
-p Threshold=threshold value of motion vector difference, which is changed ( mvd_x^2 + mv_y^2 <= threshold)
-p InsertMode=insertmode (0: no insert -> result is same original JM19,
   			  1: only modified motion vector forward;
			  2: only modify mv backward;
			  3: both forward and backward.

2. For decoder : add 3 input parameter.
-p WaterMarkFile="output file for storing embed data, which is get from bitstream"
-p Threshold=threshold value of motion vector difference, which is changed ( mvd_x^2 + mv_y^2 <= threshold)

--> Method input above parameter are same as other parameter of JM (direcly from command or from config file).

Note: For revision 412d4f1470431510da3ea0d0ffc2298e51df8540, only support insert to P slice, and Macroblock 16x16.
Example: using default config at encoder.cfg and decoder.cfg.
For encoder:
-----
ReconFile             = "test_rec.yuv"       # Reconstruction YUV file
OutputFile            = "test.264"           # Bitstream
WaterMarkFile         = "wm_file_enc.txt"    # Bitstream
Threshold	      = 2
InsertMode	      = 3
-----
./lencod.exe -f encoder.cfg

After that, using bitstream for decoder.
-----
InputFile             = "test.264"       # H.264/AVC coded bitstream
OutputFile            = "test_dec.yuv"   # Output file, YUV/RGB
WaterMarkFile         = "wm_file_dec.txt"    # Bitstream
-----
./ldecod.exe -f decoder.cfg

Result is need confirm:
1.Local decoded result at encoder phase (test_rec.yuv) is identical with decoded result at decoder phase( test_dec.yuv)
2.Input watermark file in encoder is same as output watermark file at decoder.


