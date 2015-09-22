N.B., this is still very much under development. Use at your own risk.

A C library for reading/parsing local and remote BigWig files. While Kent's source code is free to use for these purposes, it's really inappropriate as library code since it has the unfortunate habit of calling `exit()` whenever there's an error. If that's then used inside of something like python then the python interpreter gets killed. This library is aimed at resolving these sorts of issues and should also use more standard things like curl and has a friendlier license to boot.

#To do
 - [ ] Fully validate correctness of statistic calculation
   - [ ] Do Kent's tools take block partial overlap with an interval into account when determining statistics?
 - [ ] Test remote files
 - [ ] Write methods for creating bigWig files (from bedGraph like input)
